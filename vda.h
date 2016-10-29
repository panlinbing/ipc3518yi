/*
 * vda.h
 *
 *  Created on: Oct 17, 2016
 *      Author: hoang
 */

#ifndef VDA_H_TXT_
#define VDA_H_TXT_

#include <stdio.h>
#include "sample_comm.h"

typedef struct hiVDA_MD_PARAM_S
{
    HI_BOOL bThreadStart;
    VDA_CHN VdaChn;
    pthread_t stVdaPid;
}VDA_MD_PARAM_S;

int init_vda_modul(VDA_CHN stVdaChn);
int destroy_vda_modul();

int start_thread_check_motion();
int stop_thread_check_motion();

#endif /* VDA_H_TXT_ */
