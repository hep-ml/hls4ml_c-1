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

#define PROJ_HDR <MYPROJ.h>
#define PROJ_CPP <MYPROJ.cpp>

#include PROJ_HDR
#include PROJ_CPP
#include "kernel_params.h"

#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"

/*
    HLS4ML Kernel Implementation 
    Arguments:
        in    (input)     --> Input Vector
        out   (output)    --> Output Vector
   */
static void makeIn(const bigdata_t* in_arr, hls::stream<input_t>& inStream) {
    unsigned int readi[IN_STREAM_LEN];
    for(int i0 = 0; i0 < IN_STREAM_LEN; i0++) { 
      #pragma HLS LOOP UNROLL
      readi[i0] = i0/COMPRESSION;
    }
    for(int i0 = 0; i0 < IN_STREAM_LEN; i0++) { 
      input_t tmpi;
      tmpi[0].range(15,0) = in_arr[readi[i0]].range(16*((i0%COMPRESSION)+1)-1,16*(i0%COMPRESSION));
      inStream << tmpi;
    }
}

static void makeOut(bigdata_t* out_arr, hls::stream<result_t>& outStream) {
    //all this is not yet very general (e.g. it breaks if the output is wider than bigdata_t and is required to loop)
    unsigned int writei[OUT_STREAM_LEN];
    for(int i0 = 0; i0 < OUT_STREAM_LEN; i0++) { 
      #pragma HLS LOOP UNROLL
      if (i0*DATA_SIZE_OUT%COMPRESSION==COMPRESSION-1)
        writei[i0] = 3; //write 3x + reset
      else if (i0*DATA_SIZE_OUT%COMPRESSION==COMPRESSION-2)
        writei[i0] = 2; //write 2x + reset + write
      else if (i0*DATA_SIZE_OUT%COMPRESSION==COMPRESSION-3)
        writei[i0] = 1; //write + reset + write 2x
      else
        writei[i0] = 0; //write 3x
    }
    bigdata_t bigtmp;
    unsigned int outi = 0;
    for(int i0 = 0; i0 < OUT_STREAM_LEN; i0++) { 
      result_t tmpo = outStream.read();
      bigtmp.range(48*((i0%COMPRESSION)+1)-33,48*(i0%COMPRESSION)) = tmpo[0].range(15,0);
      if (writei[i0]==1) {
        out_arr[outi] = bigtmp;
        outi++;
        bigtmp = 0;
      }
      bigtmp.range(48*((i0%COMPRESSION)+1)-17,48*(i0%COMPRESSION)+16) = tmpo[1].range(15,0);
      if (writei[i0]==2) {
        out_arr[outi] = bigtmp;
        outi++;
        bigtmp = 0;
      }
      bigtmp.range(48*((i0%COMPRESSION)+1)-1,48*(i0%COMPRESSION)+32) = tmpo[2].range(15,0);
      if (writei[i0]==3) {
        out_arr[outi] = bigtmp;
        outi++;
        bigtmp = 0;
      }
    }
    out_arr[0] = bigtmp;
    
}

extern "C" {

void alveo_hls4ml(
        const bigdata_t *in, // Read-Only Vector
        bigdata_t *out       // Output Result
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
#pragma HLS INTERFACE m_axi port=in  offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem
#pragma HLS INTERFACE s_axilite port=in   bundle=control
#pragma HLS INTERFACE s_axilite port=out  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    //necessary for hls4ml kernel, not used
    #pragma HLS DATAFLOW

    unsigned short const_size_in_1, const_size_out_1;

    std::cout<<"------------------"<<std::endl;
    static hls::stream<input_t> test_in("test_in");
    #pragma HLS STREAM   variable=test_in  depth=5000
    static hls::stream<result_t> test_out("test_out");
    #pragma HLS STREAM   variable=test_out  depth=5
    //hls4ml: MYPROJ(in_buf[i],out_buf[i],const_size_in_1, const_size_out_1);
    doInputs: makeIn(in, test_in);
    hls4ml: myproject(test_in,test_out,const_size_in_1, const_size_out_1);
    doOutputs: makeOut(out, test_out);
    std::cout<<"inf end"<<std::endl;
  }
}
