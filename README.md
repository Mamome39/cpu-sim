# cpu-sim
This is a side project, I want to try deriving the knowledge from class with the help of Claude.

***About UArch***
5-stage pipeline cpu: IF ID EXE MEM WB
each pipeline contain:
    - evaluate() :combi logic
    - latch() :move combi logic to store in pipeline_reg

***What have been done***
- Base components: reg_file/ flat_mem
- rv32i-related decoding logic/ const
- pipeline stage: IF ID EXE