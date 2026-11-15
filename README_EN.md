# Sheet Pile Wall Design by Limit State Method

A program for the design of sheet pile walls using the limit state method.

🇷🇺 [Русская версия этого README](README.md)

## How to use

1. Download the program from the [Releases](../../releases) page.
2. Run the executable file `shpunt_limit_state.exe`.
3. Select the design scheme from the drop-down list.
4. Fill in all the tabs on the left with your parameters one by one. Alternatively, click "Load parameters…" if you have a ready-made file with parameters.
5. Click "Calculate" (or press Enter in any field).

## Notes from the author

While working on the program, I discovered that the calculation result depends very strongly on the coefficient m in the check M_opr ≤ m·M_ud. The threshold value is m = 0.75: if m < 0.75, there is no solution regardless of the pit height; if m > 0.75, there is always a solution. I believe the scientific community is aware of this, and that is precisely why the coefficients have to be adjusted so that the mathematical model corresponds to reality. For the same reasons, I introduced an additional parameter, h_ratio_min — a check for h/H ≥ 0.45. The value of 0.45 is well suited for sand; for clay soils, it makes sense to use 0.5–0.6. But you understand yourself: the soil is not homogeneous; sandy loam may alternate with clay, so it is impossible to choose a mathematical model precisely. Therefore, it is better to use my program for calculating sheet piles using the finite element method — this mathematical model is more universal.

