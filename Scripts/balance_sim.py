"""AIGame 밸런스 간이 시뮬레이션 (GDD 리스크 4: 시드 고정 시뮬레이션으로 일차별 순이익 곡선 확인)

Data/*.csv를 읽어, docs/02_design/architecture.md 3.6의 영업 규칙을 단순화해 따라간다.
실행: python Scripts/balance_sim.py [--seeds 30] [--days 10]

플레이어 모델(탭 간격 H초 · 제품마다 판단 D초 · 재료 탭마다 실수 확률 ERR/2)
  - 대기열에서 주문이 정해진 첫 손님의 조합을 만든다. 없으면 기본 베이스 단일을 만든다.
  - 실수하면 그 제품은 쓰레기(그때까지 낸 재료비 손실)
  - 로비에서는 살 수 있는 것(다음 해금 · 찜기) 중 싼 것부터 계속 산다.
한계(실제 게임과 다른 점)
  - 미리 만들어 쌓아 두기를 하지 않는다.
  - 완성품 배분을 찜기 탭 시점에 감싸기 끝 시각 기준으로 근사한다.
  - 시간은 0.05초 단위로 진행한다(게임은 이벤트 힙 + 타이머 1개).
"""
import argparse
import csv
import math
import os
import random
import statistics

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Data')
BASES = {'RedBean', 'Yogurt', 'DubaiChoco'}  # 코드의 IsBaseIngredient와 같게 유지한다
PROFILES = [
    ('보통', dict(H=0.5, D=0.5, ERR=0.05)),
    ('느림', dict(H=0.8, D=1.0, ERR=0.10)),
    ('서툰', dict(H=1.0, D=1.5, ERR=0.20)),
    ('실수 많음', dict(H=0.8, D=1.0, ERR=0.33)),
    ('실수 아주 많음', dict(H=0.8, D=1.0, ERR=0.45)),
]
STEP = 0.05


def read_rows(name):
    with open(os.path.join(DATA_DIR, name), encoding='utf-8-sig', newline='') as f:
        return {row['---']: row for row in csv.DictReader(f)}


def load():
    ing = {k: dict(base=k in BASES, order=int(r['UnlockOrder']), unlock=int(r['UnlockCost']), cost=int(r['Cost']),
                   price=int(r['Price']), weight=int(r['OrderWeight']), rent_inc=int(r['RentIncrease']))
           for k, r in read_rows('Ingredients.csv').items()}
    days = [dict(dur=float(r['Duration']), amin=float(r['ArrivalMin']), amax=float(r['ArrivalMax']),
                 patience=float(r['Patience']), maxq=int(r['MaxQueue']), anyrate=float(r['AnyOrderRate']))
            for _, r in sorted(read_rows('Days.csv').items(), key=lambda kv: int(kv[0]))]
    steam = [dict(gauge=int(r['MaxGauge']), regen=float(r['RegenTime']), cost=int(r['UpgradeCost']))
             for _, r in sorted(read_rows('Steamer.csv').items(), key=lambda kv: int(kv[0]))]
    b = read_rows('Balance.csv')['Default']
    bal = dict(start=int(b['StartGold']), rice=int(b['RiceCakeCost']), nofruit=int(b['NoFruitWeight']),
               mix=float(b['MixMultiplier']), place=float(b['PlaceTime']), wrap=float(b['WrapTime']),
               trash=float(b['TrashTime']), maxstock=int(b['MaxStockPerRecipe']), rent=int(b['BaseRent']))
    return ing, days, steam, bal


def price(ing, bal, recipe):
    base, fruit = recipe
    if fruit is None:
        return ing[base]['price']
    return math.floor((ing[base]['price'] + ing[fruit]['price']) * bal['mix'] + 0.5)  # FMath::RoundToInt와 같이 0.5 올림


def weighted(rng, items):
    total = sum(w for _, w in items)
    pick = rng.random() * total
    for item, w in items:
        pick -= w
        if pick < 0:
            return item
    return items[-1][0]


def run_day(data, day, unlock_step, steam_lv, seed, H, D, ERR):
    ing, days, steam, bal = data
    rng = random.Random(seed)
    d = days[min(day, len(days)) - 1]
    st = steam[steam_lv - 1]
    unlocked = [k for k, v in ing.items() if v['order'] <= unlock_step]
    bases = sorted((k for k in unlocked if ing[k]['base']), key=lambda k: ing[k]['order'])
    fruits = [k for k in unlocked if not ing[k]['base']]
    queue, shelf = [], {}
    res = dict(sales=0, cost=0, sold=0, missed=0, trash=0, discard=0)
    gauge, refill_end = st['gauge'], None
    now, next_arrival, busy_until, plan = 0.0, rng.uniform(d['amin'], d['amax']), 0.0, []

    def sell(recipe):
        res['sales'] += price(ing, bal, recipe)
        res['sold'] += 1

    while now < d['dur']:
        while next_arrival <= now:
            any_order = rng.random() < d['anyrate']
            order = None if any_order else (
                weighted(rng, [(b, ing[b]['weight']) for b in bases]),
                weighted(rng, [(None, bal['nofruit'])] + [(f, ing[f]['weight']) for f in fruits]))
            stocked = [k for k, c in shelf.items() if c > 0]
            if any_order and stocked:
                k = rng.choice([k for k in stocked for _ in range(shelf[k])])  # 개수 비례(D12)
                shelf[k] -= 1
                sell(k)
            elif not any_order and shelf.get(order, 0) > 0:  # 대기열이 꽉 차도 바로 구매(D13)
                shelf[order] -= 1
                sell(order)
            elif len(queue) < d['maxq']:
                queue.append(dict(order=order, expire=next_arrival + d['patience']))
            next_arrival += rng.uniform(d['amin'], d['amax'])
        for c in [c for c in queue if c['expire'] <= now]:
            queue.remove(c)
            res['missed'] += 1
        if refill_end is not None and refill_end <= now:
            gauge, refill_end = st['gauge'], None
        if now >= busy_until:
            if not plan and queue:
                target = next((c for c in queue if c['order'] is not None), None)
                base, fruit = target['order'] if target else (bases[0], None)
                plan = [('ing', base)] + ([('ing', fruit)] if fruit else []) + [('steam', (base, fruit))]
                busy_until = now + D
            elif plan and not (plan[0][0] == 'steam' and gauge == 0):
                kind, value = plan.pop(0)
                if kind == 'ing':
                    res['cost'] += ing[value]['cost']
                    if rng.random() < ERR / 2:
                        res['trash'] += 1
                        plan = []
                        busy_until = now + H + bal['trash']
                    else:
                        busy_until = now + H + bal['place']
                else:
                    gauge -= 1
                    res['cost'] += bal['rice']
                    if gauge == 0:
                        refill_end = now + st['regen']
                    done = now + H + bal['wrap']
                    busy_until = done
                    match = next((c for c in queue if c['order'] is None or c['order'] == value), None)
                    if match and match['expire'] > done:
                        queue.remove(match)
                        sell(value)
                    elif shelf.get(value, 0) < bal['maxstock']:
                        shelf[value] = shelf.get(value, 0) + 1
                    else:
                        res['discard'] += 1
        now += STEP
    res['discard'] += sum(shelf.values())
    return res


def daily_rent(ing, bal, unlock_step):
    """기본 임대료 + 해금된 재료의 RentIncrease 합 (ADR-0017)"""
    return bal['rent'] + sum(v['rent_inc'] for v in ing.values() if v['order'] <= unlock_step)


def campaign(data, seed, days, profile):
    ing, _, steam, bal = data
    gold, unlock_step, steam_lv, rows, prev_debt = bal['start'], 0, 1, [], False
    for day in range(1, days + 1):
        rent = daily_rent(ing, bal, unlock_step)
        r = run_day(data, day, unlock_step, steam_lv, seed * 100 + day, **profile)
        r['rent'] = rent
        gold += r['sales'] - r['cost'] - rent  # 임대료는 영업 시작 때(D17), 결산 순이익에 포함
        game_over = prev_debt and gold < 0
        prev_debt = gold < 0
        while not game_over:
            nxt = [k for k, v in ing.items() if v['order'] == unlock_step + 1]
            options = [(c, n) for c, n in ((ing[nxt[0]]['unlock'] if nxt else None, 'unlock'),
                                           (steam[steam_lv]['cost'] if steam_lv < len(steam) else None, 'steam'))
                       if c is not None and c <= gold]
            if not options:
                break
            c, n = min(options)
            gold -= c
            if n == 'unlock':
                unlock_step += 1
            else:
                steam_lv += 1
        rows.append(dict(day=day, gold=gold, unlock=unlock_step, steam=steam_lv, over=game_over, **r))
        if game_over:
            break
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seeds', type=int, default=30)
    ap.add_argument('--days', type=int, default=10)
    args = ap.parse_args()
    data = load()
    for label, profile in PROFILES:
        runs = [campaign(data, s, args.days, profile) for s in range(1, args.seeds + 1)]
        print(f"== {label} (탭 {profile['H']}s · 판단 {profile['D']}s · 탭당 실수 {profile['ERR'] / 2:.1%})")
        print('일차 | 판매/놓침 | 매출 | 재료비 | 임대료 | 순이익 | 구매 뒤 골드 | 해금 | 찜기 | 쓰레기 | 폐기')
        for day in range(1, args.days + 1):
            rs = [r[day - 1] for r in runs if len(r) >= day]
            if not rs:
                break
            m = lambda key: statistics.mean(x[key] for x in rs)
            print(f"{day:>4} | {m('sold'):5.1f}/{m('missed'):4.1f} | {m('sales'):5.0f} | {m('cost'):5.0f} | {m('rent'):5.0f} | "
                  f"{m('sales') - m('cost') - m('rent'):5.0f} | {m('gold'):7.0f} | {m('unlock'):3.1f} | {m('steam'):3.1f} | "
                  f"{m('trash'):4.1f} | {m('discard'):3.1f}")
        done = [next((x['day'] for x in r if x['unlock'] >= 6), None) for r in runs]
        finished = [x for x in done if x]
        warned = sum(1 for r in runs if any(x['gold'] < 0 for x in r))
        bankrupt = [(r[-1]['day'], r[-1]['unlock']) for r in runs if r[-1]['over']]
        print(f"  6단계 해금 완료: {statistics.mean(finished):.1f}일차" if finished else '  6단계 해금 완료: 없음', end='')
        print(f" (미완료 {len(done) - len(finished)}/{args.seeds}) · 빚 경고 경험 {warned}/{args.seeds} · "
              f"파산 {len(bankrupt)}/{args.seeds} (일차 · 해금 단계: {sorted(bankrupt)})")


if __name__ == '__main__':
    main()
