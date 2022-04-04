/*
 * Copyright (c) 2016-2018, University of Bristol - http://www.bristol.ac.uk
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/**
 * \file
 *         Header file for specksense
 * \author
 *         Wenbo Xiao <xwb97@icloud.com>
 */

#ifndef SPECKSENSE_H__
#define SPECKSENSE_H__


/**
 * \brief Initializes the rssi quantization levels.
 */
void init_power_levels(void);
    
/**
 * \brief RSSI sampler stores power level and duration into 2D vector, will stop if time is not enough for another sample.
 * \param sample_amount     limited the maximum sample amount to avoid getting stuck
 * \param channel     channel to do rssion
 * \param RSSI_time     time to stop RSSI
 */
void rssi_sampler(int sample_amount, int channel, rtimer_clock_t rssi_stop_time);

/**
 * \brief add channel that needs to do RSSI on to queue
 * \param channel     channel from tsch sequence
 */
void specksense_channel_add(unsigned int channel);

/**
 * \brief remove first channel on queue
 */
void specksense_channel_remove(void);

/**
 * \brief remove channel that needs to do RSSI on to queue
 * \retval first channel that will do RSSI, 0 if empty.
 */
int specksense_channel_peek(void);

/**
 * \brief Runs specksense module
 * \retval  return 0 if not enough sample; 
 *          return 1 if having enough sample, do classification. And sample will be cleared if classification is done.
 */
int specksense_process();

/**
 * \brief Runs jammer trigger module
 */
void jammer_trigger_process(void);

/**
 * \brief Runs classification module
 */
void classification_process(void);

#endif /* __TSCH_CS_H__ */
