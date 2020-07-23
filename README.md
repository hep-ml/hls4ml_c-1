# DeepCalo (broken) with Vitis 2019.2 (HLS C/C++ Kernel)

Setup tools, licenses, check connection to FPGA card

Logs from previous test w/ errors:
```bash
out.log
_x.hw.xilinx_u250_xdma_201830_2/alveo_hls4ml/alveo_hls4ml/vivado_hls.log
```

To replicate bitcode error:
```bash
nohup make check TARGETS=hw DEVICE=xilinx_u250_xdma_201830_2 all
```
