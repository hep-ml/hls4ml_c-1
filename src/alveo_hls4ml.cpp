/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

/*******************************************************************************
Description:
    HLS pragmas can be used to optimize the design : improve throughput, reduce latency and 
    device resource utilization of the resulting RTL code
    This is a wrapper to be used with an hls4ml project to enable proper handling by SDAccel
*******************************************************************************/
#include "ap_axi_sdata.h"
#include "hls_stream.h"

#define PROJ_HDR <MYPROJ.h>

#include PROJ_HDR
#include "kernel_params.h"

#define DWIDTH 512
#define DATATYPE_SIZE 16
#define VECTOR_SIZE (DWIDTH / DATATYPE_SIZE) // vector size is 32 (512/16 = 32)

typedef qdma_axis<DWIDTH, 0, 0, 0> pkt;

/*
    HLS4ML Kernel Implementation 
    Arguments:
        in    (input)     --> Input Vector
        out   (output)    --> Output Vector
   */
extern "C" {
void alveo_hls4ml(
        hls::stream<pkt> &in, // Read-Only Vector
        hls::stream<pkt> &out       // Output Result
        )
{
// SDAccel kernel must have one and only one s_axilite interface which will be used by host application to configure the kernel.
// Here bundle control is defined which is s_axilite interface and associated with all the arguments (in and out),
// control interface must also be associated with "return".
// All the global memory access arguments must be associated to one m_axi(AXI Master Interface). Here all two arguments(in, out) are 
// associated to bundle gmem which means that a AXI master interface named "gmem" will be created in Kernel and all these variables will be 
// accessing global memory through this interface.
// Multiple interfaces can also be created based on the requirements. For example when multiple memory accessing arguments need access to
// global memory simultaneously, user can create multiple master interfaces and can connect to different arguments.
#pragma HLS INTERFACE axis port=in
#pragma HLS INTERFACE axis port=out
#pragma HLS INTERFACE s_axilite port=return bundle=control


    for (int i0 = 0; i0 < COMPSTREAMSIZE; i0++) {
        pkt t_out;
        bigdata_t tmpOut;
        for (int i = 0; i < COMPRESSION; i++) {
            pkt t1 = in.read();
            bigdata_t dtmp = t1.get_data();

            input_t in_buf[DATA_SIZE_IN];
            layer11_t out_buf[DATA_SIZE_OUT];
            //these will get partitioned properly in the hls4ml code
            #pragma HLS ARRAY_RESHAPE   variable=in_buf  complete
            #pragma HLS ARRAY_RESHAPE   variable=out_buf complete

            for (int j = 0; j < DATA_SIZE_IN; j++) {
                #pragma HLS PIPELINE II=1
	        in_buf[j].range(15,0) = dtmp.range(16*(j+1)-1,16*j);
            }

            hls4ml: MYPROJ(in_buf,out_buf);
	
            for(int j = 0; j < DATA_SIZE_OUT; j++) { 
                tmpOut((i+1)*16-1,(i)*16) = out_buf[i0*COMPRESSION+i][j].range(15,0);
            }
        }

        // Setting data and configuration to output packet
        t_out.set_data(tmpOut);
        t_out.set_last(i0==(COMPSTREAMSIZE-1) ? 1 : 0);
        t_out.set_keep(-1); // Enabling all bytes

        // Writing packet to output stream
        output.write(t_out);

    }
}
}
