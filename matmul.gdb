b breakpoint
commands 1
    #shell bash -c "cd ~/npu/ops_reg/ && python3 dump.py 1" 
    shell bash -c "cd ~/npu/ops_reg/ && python3 dump.py 2" | grep -v 0x00000000
    #shell bash -c "cd ~/npu/ops_reg/ && python3 dump.py 3" 
    #shell bash -c "cd ~/npu/ops_reg/ && python3 dump.py 4" 
    c
end

r
q