# Plan Generator Instruction

This document contains the original instruction for generating the terrain renderer implementation plan.

## Original Instruction

Read the L1inst.md file which contains a generated instruction for turning the baseline project into a simple terrain renderer with water. Use this instruction and compare it to existing files and write a L1inst_plan.md that will encapsulate more detailed changes and implementations required to turn baseline into the terrain renderer.

The main important note to be made is that when writing the plan, you should avoid creation of additional header files. You should make all the main changes inside the main .cpp (in the baseline case it is Baseline.cpp).

But keep it in mind and make another smaller instruction that keeps track of added functionality and can be used to quickly dissect the additions into separate .cpp and header files.

Make the resulting plan well managed with a lot of commentary and structural markups. Use Foam addon functionality to structure it.

But keep in mind that the resulting plan will be used with Cursor to continue the development, so before all else copy this instruction into new_plan_generator.md.

## Key Requirements

1. **Single File Approach**: All main changes should be in Baseline.cpp
2. **Detailed Implementation**: Line-by-line changes with exact code
3. **Foam Structure**: Use Foam-style markdown with wikilinks and structure
4. **Refactoring Guide**: Separate document for later extraction into separate files
5. **Well Commented**: Extensive commentary explaining each change
6. **Cursor Compatible**: Plan should be usable directly with Cursor AI












