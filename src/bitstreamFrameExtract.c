//MIT License
//Copyright (c) 2023 Jared Loewenthal
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.


//This is the main file for the Vulkan Video Playback program for x64
//processor architecture targets and is designed to be compiled by gcc
//This top-level program file includes the entry point and OS independent code
//When OS dependent functionality needs to be used, the linked in compatibility
//functions get called which share the same function definitions (prototypes)
//These functions make their own calls to the OS API and abstract it away from
//the main program (sometimes the majority of the code executed are in these functions)
//The program avoids using any runtime library functions which usually vary
//between OSes and insteads calls specific implementations for relevant functions
// More Details Here in the Future

#define COMPATIBILITY_NETWORK_UNNEEDED //Do not need networking
#include "programEntry.h" //Includes "programStrings.h" & "compatibility.h" & <stdint.h>

static uint64_t bitstreamGetValueAndAdvance(uint8_t** bitstreamPtr, uint64_t valueBytes, uint64_t advanceBytes) {
	uint64_t value = 0;
	uint8_t* bitstreamBytes = *bitstreamPtr;
	
	for (uint64_t i=0; i<valueBytes; i++) {
		if (*(bitstreamBytes) == 0x03) {
			if (*(bitstreamBytes - 1) == 0x00) {
				if (*(bitstreamBytes - 2) == 0x00) {
					bitstreamBytes++;
				}
			}
		}
		
		value <<= 8;
		value |= (uint64_t) (*bitstreamBytes);
		
		bitstreamBytes++;
	}
	
	for (uint64_t i=0; i<advanceBytes; i++) {
		if (*(bitstreamBytes) == 0x03) {
			if (*(bitstreamBytes - 1) == 0x00) {
				if (*(bitstreamBytes - 2) == 0x00) {
					bitstreamBytes++;
				}
			}
		}
		
		bitstreamBytes++;
	}
	
	*bitstreamPtr = bitstreamBytes;
	return value;
}

static uint64_t bitstreamGetBitValue(uint8_t** bitstreamPtr, uint64_t* bitPtr, uint64_t numBits) {
	uint64_t value = 0;
	uint8_t* bitstreamBytes = *bitstreamPtr;
	uint64_t bit = *bitPtr;
	
	if (bit == 0x80) { //Necessary?
		if (*(bitstreamBytes) == 0x03) {
			if (*(bitstreamBytes - 1) == 0x00) {
				if (*(bitstreamBytes - 2) == 0x00) {
					bitstreamBytes++;
				}
			}
		}
	}
	
	for (uint64_t i=0; i<numBits; i++) {
		value <<= 1;
		if (((*bitstreamBytes) & bit) > 0) {
			value |= 1;
		}
		
		if (bit > 1) {
			bit >>= 1;
		}
		else {
			bit = 0x80;
			bitstreamBytes++;
			if (*(bitstreamBytes) == 0x03) {
				if (*(bitstreamBytes - 1) == 0x00) {
					if (*(bitstreamBytes - 2) == 0x00) {
						bitstreamBytes++;
					}
				}
			}
		}
	}
	
	*bitPtr = bit;
	*bitstreamPtr = bitstreamBytes;
	return value;
}

static uint64_t bitstreamGetExpGolombUnsignedValue(uint8_t** bitstreamPtr, uint64_t* bitPtr) {
	uint64_t value = 0;
	uint8_t* bitstreamBytes = *bitstreamPtr;
	uint64_t bit = *bitPtr;
	
	if (bit == 0x80) { //Necessary?
		if (*(bitstreamBytes) == 0x03) {
			if (*(bitstreamBytes - 1) == 0x00) {
				if (*(bitstreamBytes - 2) == 0x00) {
					bitstreamBytes++;
				}
			}
		}
	}
	
	uint64_t zeroCount = 0;
	while (((*bitstreamBytes) & bit) == 0) {
		zeroCount++;
		
		if (bit > 1) {
			bit >>= 1;
		}
		else {
			bit = 0x80;
			bitstreamBytes++;
			if (*(bitstreamBytes) == 0x03) {
				if (*(bitstreamBytes - 1) == 0x00) {
					if (*(bitstreamBytes - 2) == 0x00) {
						bitstreamBytes++;
					}
				}
			}
		}
	}
	
	if (bit > 1) {
		bit >>= 1;
	}
	else {
		bit = 0x80;
		bitstreamBytes++;
		if (*(bitstreamBytes) == 0x03) {
			if (*(bitstreamBytes - 1) == 0x00) {
				if (*(bitstreamBytes - 2) == 0x00) {
					bitstreamBytes++;
				}
			}
		}
	}
	
	for (uint64_t i=0; i<zeroCount; i++) {
		value <<= 1;
		if (((*bitstreamBytes) & bit) > 0) {
			value |= 1;
		}
		
		if (bit > 1) {
			bit >>= 1;
		}
		else {
			bit = 0x80;
			bitstreamBytes++;
			if (*(bitstreamBytes) == 0x03) {
				if (*(bitstreamBytes - 1) == 0x00) {
					if (*(bitstreamBytes - 2) == 0x00) {
						bitstreamBytes++;
					}
				}
			}
		}
	}
	
	if (zeroCount > 0) {
		value += (1 << zeroCount) - 1;
	}	
	
	*bitPtr = bit;
	*bitstreamPtr = bitstreamBytes;
	return value;
}

static int64_t bitstreamGetExpGolombSignedValue(uint8_t** bitstreamPtr, uint64_t* bitPtr) {
	int64_t value = (int64_t) bitstreamGetExpGolombUnsignedValue(bitstreamPtr, bitPtr);
	value++;
	int64_t bit = value & 0x1;
	value >>= 1;
	if (bit == 1) {
		value *= -1;
	}	
	
	return value;
}

static StdVideoH265VideoParameterSet vps;
static StdVideoH265ProfileTierLevel ptl;
static StdVideoH265DecPicBufMgr decPicBuf = {0}; //Fill in zeros
static StdVideoH265SequenceParameterSet sps;
static StdVideoH265ProfileTierLevel spsPTL;
static StdVideoH265DecPicBufMgr spsDecPicBuf = {0}; //Fill in zeros
static StdVideoH265ShortTermRefPicSet strps[1] = {}; //Set to zero...?
static StdVideoH265SequenceParameterSetVui spsVui;
static StdVideoH265PictureParameterSet pps;

int readBitstreamParameters(uint8_t** memPtr) {
	//VPS
	uint8_t* bitstreamBytes = *memPtr;
	uint64_t* startCodeCheck = (uint64_t*) bitstreamBytes;
	if ((*startCodeCheck & 0xFFFFFFFFFFFF) != 0x014001000000) { //VPS
		return ERROR_PARSE_ISSUE;
	}
	bitstreamBytes += 6;
	
	uint64_t u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 2, 2);
	vps.vps_video_parameter_set_id = (u64 >> 12) & 0xF;
	vps.vps_max_sub_layers_minus1 = (u64 >> 1) & 0x7;
	vps.flags.vps_temporal_id_nesting_flag = u64 & 0x1;
	
	//PTL always...?	
	u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 8, 3);
	ptl.flags.general_tier_flag = (u64 >> 61) & 0x1;
	ptl.general_profile_idc = (u64 >> 56) & 0x1F;
	ptl.flags.general_progressive_source_flag = (u64 >> 24) & 0x1;
	ptl.flags.general_interlaced_source_flag = (u64 >> 23) & 0x1;
	ptl.flags.general_non_packed_constraint_flag = (u64 >> 22) & 0x1;
	ptl.flags.general_frame_only_constraint_flag = (u64 >> 21) & 0x1;
	
	u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 1, 0);
	ptl.general_level_idc = u64 & 0xFF;
	
	if (vps.vps_max_sub_layers_minus1 > 0) {
		u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 2, 0);
		
		uint8_t bitShift = 15;
		for (uint8_t i=0; i<vps.vps_max_sub_layers_minus1; i++) {
			if (((u64 >> bitShift) & 1) == 1) {
				bitstreamGetValueAndAdvance(&bitstreamBytes, 0, 11);
			}
			bitShift--;
			if (((u64 >> bitShift) & 1) == 1) {
				bitstreamGetValueAndAdvance(&bitstreamBytes, 0, 1);
			}
			bitShift--;
		}
	}
	
	vps.pProfileTierLevel = &ptl;
	
	//consoleWriteLineWithNumberFast("Val: ", 5, ptl.general_level_idc, NUM_FORMAT_UNSIGNED_INTEGER);	
	//consoleWriteLineWithNumberFast("Num: ", 5, *bitstreamBytes, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	uint64_t bit = 0x80;
	vps.flags.vps_sub_layer_ordering_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	
	//decPicBuf
	for (uint8_t i = (vps.flags.vps_sub_layer_ordering_info_present_flag ? 0 : vps.vps_max_sub_layers_minus1);
		i <= vps.vps_max_sub_layers_minus1; i++) {
		
		//consoleWriteLineWithNumberFast("Val: ", 5, vps.vps_max_sub_layers_minus1, NUM_FORMAT_UNSIGNED_INTEGER);
		
		decPicBuf.max_dec_pic_buffering_minus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		decPicBuf.max_num_reorder_pics[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		decPicBuf.max_latency_increase_plus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	}
	vps.pDecPicBufMgr = &decPicBuf;
	
	uint64_t t0 = bitstreamGetBitValue(&bitstreamBytes, &bit, 6); //vps_max_layer_id
	//consoleWriteLineWithNumberFast("Num: ", 5, *bitstreamBytes, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Bit: ", 5, bit, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Val: ", 5, t0, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	uint64_t t1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit); //vps_num_layer_sets_minus1
	for (uint64_t i = 1; i <= t1; i++) {
		for (uint64_t j = 0; j <= t0; j++) {
			bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //layer_id_included_flag
		}
	}
	
	vps.flags.vps_timing_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (vps.flags.vps_timing_info_present_flag == 1) {
		vps.vps_num_units_in_tick = bitstreamGetBitValue(&bitstreamBytes, &bit, 32);
		vps.vps_time_scale = bitstreamGetBitValue(&bitstreamBytes, &bit, 32);
		vps.flags.vps_poc_proportional_to_timing_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (vps.flags.vps_poc_proportional_to_timing_flag == 1) {
			vps.vps_num_ticks_poc_diff_one_minus1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		}
		else {
			vps.vps_num_ticks_poc_diff_one_minus1 = 0;
		}
		u64 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		if (u64 > 0) {
			return ERROR_PARSE_ISSUE; //TODO
		}
		else {
			vps.pHrdParameters = NULL;
		}
	}
	else {
		vps.vps_num_units_in_tick = 0;
		vps.vps_time_scale = 0;
		vps.flags.vps_poc_proportional_to_timing_flag = 0;
		vps.vps_num_ticks_poc_diff_one_minus1 = 0;
		vps.pHrdParameters = NULL;
	}
	
	u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //vps_extension_flag
	if (u64 == 1) {
		return ERROR_PARSE_ISSUE; //TODO
	}
	
	//Byte alignment
	u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //stop bit
	if (u64 != 1) {
		return ERROR_PARSE_ISSUE; //TODO
	}
	if (bit != 0x80) {
		bitstreamBytes++;
		bit = 0x80;
	}
	
	vps.reserved1 = 0;
	vps.reserved2 = 0;
	vps.reserved3 = 0;
	//VPS finished setup
	
	
	
	startCodeCheck = (uint64_t*) bitstreamBytes;
	if ((*startCodeCheck & 0xFFFFFFFFFFFF) != 0x014201000000) { //SPS
		return ERROR_PARSE_ISSUE;
	}
	bitstreamBytes += 6;
		
	
	
	u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 1, 0);
	sps.sps_video_parameter_set_id = (u64 >> 4) & 0xF;
	sps.sps_max_sub_layers_minus1 = (u64 >> 1) & 0x7;
	sps.flags.sps_temporal_id_nesting_flag = u64 & 0x1;
	
	
	
	u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 8, 3);
	spsPTL.flags.general_tier_flag = (u64 >> 61) & 0x1;
	spsPTL.general_profile_idc = (u64 >> 56) & 0x1F;
	spsPTL.flags.general_progressive_source_flag = (u64 >> 24) & 0x1;
	spsPTL.flags.general_interlaced_source_flag = (u64 >> 23) & 0x1;
	spsPTL.flags.general_non_packed_constraint_flag = (u64 >> 22) & 0x1;
	spsPTL.flags.general_frame_only_constraint_flag = (u64 >> 21) & 0x1;
	
	u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 1, 0);
	spsPTL.general_level_idc = u64 & 0xFF;
	
	if (sps.sps_max_sub_layers_minus1 > 0) {
		u64 = bitstreamGetValueAndAdvance(&bitstreamBytes, 2, 0);
		
		uint8_t bitShift = 15;
		for (uint8_t i=0; i<sps.sps_max_sub_layers_minus1; i++) {
			if (((u64 >> bitShift) & 1) == 1) {
				bitstreamGetValueAndAdvance(&bitstreamBytes, 0, 11);
			}
			bitShift--;
			if (((u64 >> bitShift) & 1) == 1) {
				bitstreamGetValueAndAdvance(&bitstreamBytes, 0, 1);
			}
			bitShift--;
		}
	}
	
	sps.pProfileTierLevel = &spsPTL;
	
	sps.sps_seq_parameter_set_id = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	sps.chroma_format_idc = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	if (sps.chroma_format_idc == 3) {
		sps.flags.separate_colour_plane_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	}
	else {
		sps.flags.separate_colour_plane_flag = 0;
	}
	
	sps.pic_width_in_luma_samples = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
  sps.pic_height_in_luma_samples = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	consoleWriteLineWithNumberFast("Val: ", 5, sps.pic_width_in_luma_samples, NUM_FORMAT_UNSIGNED_INTEGER);
	consoleWriteLineWithNumberFast("Val: ", 5, sps.pic_height_in_luma_samples, NUM_FORMAT_UNSIGNED_INTEGER);
  
	
	sps.flags.conformance_window_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (sps.flags.conformance_window_flag == 1) {
		sps.conf_win_left_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		sps.conf_win_right_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		sps.conf_win_top_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		sps.conf_win_bottom_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	}
	else {
		sps.conf_win_left_offset = 0;
		sps.conf_win_right_offset = 0;
		sps.conf_win_top_offset = 0;
		sps.conf_win_bottom_offset = 0;
	}
  
	sps.bit_depth_luma_minus8 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	sps.bit_depth_chroma_minus8 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	
	sps.log2_max_pic_order_cnt_lsb_minus4 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	
	
	sps.flags.sps_sub_layer_ordering_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	for (uint8_t i = (sps.flags.sps_sub_layer_ordering_info_present_flag ? 0 : sps.sps_max_sub_layers_minus1);
		i <= sps.sps_max_sub_layers_minus1; i++) {		
		
		spsDecPicBuf.max_dec_pic_buffering_minus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		spsDecPicBuf.max_num_reorder_pics[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		spsDecPicBuf.max_latency_increase_plus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	}
	sps.pDecPicBufMgr = &spsDecPicBuf;
  
	sps.log2_min_luma_coding_block_size_minus3 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	sps.log2_diff_max_min_luma_coding_block_size = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	sps.log2_min_luma_transform_block_size_minus2 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	sps.log2_diff_max_min_luma_transform_block_size = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	sps.max_transform_hierarchy_depth_inter = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	sps.max_transform_hierarchy_depth_intra = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	
	//SPS Scaling
	sps.flags.scaling_list_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (sps.flags.scaling_list_enabled_flag == 1) {
		sps.flags.sps_scaling_list_data_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (sps.flags.scaling_list_enabled_flag == 1) {
			return ERROR_PARSE_ISSUE; //TODO
		}
		else {
			sps.pScalingLists = NULL;
		}
	}
	else {
		sps.flags.sps_scaling_list_data_present_flag = 0;
		sps.pScalingLists = NULL;
	}
	
  sps.flags.amp_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  sps.flags.sample_adaptive_offset_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  sps.flags.pcm_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (sps.flags.pcm_enabled_flag == 1) {
		sps.pcm_sample_bit_depth_luma_minus1 = bitstreamGetBitValue(&bitstreamBytes, &bit, 4);
		sps.pcm_sample_bit_depth_chroma_minus1 = bitstreamGetBitValue(&bitstreamBytes, &bit, 4);
		sps.log2_min_pcm_luma_coding_block_size_minus3 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		sps.log2_diff_max_min_pcm_luma_coding_block_size = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		sps.flags.pcm_loop_filter_disabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	}
	else {
		sps.pcm_sample_bit_depth_luma_minus1 = 0;
		sps.pcm_sample_bit_depth_chroma_minus1 = 0;
		sps.log2_min_pcm_luma_coding_block_size_minus3 = 0;
		sps.log2_diff_max_min_pcm_luma_coding_block_size = 0;
		sps.flags.pcm_loop_filter_disabled_flag = 0;
	}
	
	
	//Short Term Reference Pictures:
	sps.num_short_term_ref_pic_sets = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	if (sps.num_short_term_ref_pic_sets == 0) {
		sps.pShortTermRefPicSet = NULL;
	}
	else if (sps.num_short_term_ref_pic_sets == 1) {
		// Due to only 1 reference picture set:
		strps[0].flags.inter_ref_pic_set_prediction_flag = 0;
		strps[0].flags.delta_rps_sign = 0;
		strps[0].delta_idx_minus1 = 0;
		strps[0].use_delta_flag = 0;
		strps[0].abs_delta_rps_minus1 = 0;
		strps[0].used_by_curr_pic_flag = 0;
		
		
		//Max of 16
		strps[0].num_negative_pics = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		strps[0].num_positive_pics = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		
		strps[0].used_by_curr_pic_s0_flag = 0;
		for (uint64_t i = 0; i < strps[0].num_negative_pics; i++) {
			strps[0].delta_poc_s0_minus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			strps[0].used_by_curr_pic_s0_flag <<= 1;
			strps[0].used_by_curr_pic_s0_flag |= bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		}
		
		strps[0].used_by_curr_pic_s1_flag = 0;
		for (uint64_t i = 0; i < strps[0].num_positive_pics; i++) {
			strps[0].delta_poc_s1_minus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			strps[0].used_by_curr_pic_s1_flag <<= 1;
			strps[0].used_by_curr_pic_s1_flag |= bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		}	
		
		strps[0].reserved1 = 0;
		strps[0].reserved2 = 0;
		strps[0].reserved3 = 0;
		
		sps.pShortTermRefPicSet = strps;
	}
	else {
		return ERROR_PARSE_ISSUE; //TODO
	}
	
  //Long term reference pictures
	sps.flags.long_term_ref_pics_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (sps.flags.long_term_ref_pics_present_flag == 1) {
		sps.num_long_term_ref_pics_sps = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		if (sps.num_long_term_ref_pics_sps > 0) {
			return ERROR_PARSE_ISSUE; //TODO
		}
		else {
			sps.pLongTermRefPicsSps = NULL;
		}
	}
	else {
		sps.num_long_term_ref_pics_sps = 0;
		sps.pLongTermRefPicsSps = NULL;
	}
  
	
  sps.flags.sps_temporal_mvp_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  sps.flags.strong_intra_smoothing_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	
	
	
  sps.flags.vui_parameters_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (sps.flags.vui_parameters_present_flag == 1) {
		spsVui.flags.aspect_ratio_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (spsVui.flags.aspect_ratio_info_present_flag == 1) {
			spsVui.aspect_ratio_idc = bitstreamGetBitValue(&bitstreamBytes, &bit, 8);
			if (spsVui.aspect_ratio_idc == STD_VIDEO_H265_ASPECT_RATIO_IDC_EXTENDED_SAR) {
				spsVui.sar_width = bitstreamGetBitValue(&bitstreamBytes, &bit, 16);
				spsVui.sar_height = bitstreamGetBitValue(&bitstreamBytes, &bit, 16);
			}
			else {
				spsVui.sar_width = 0;
				spsVui.sar_height = 0;
			}
		}
		else {
			spsVui.aspect_ratio_idc = 0;
			spsVui.sar_width = 0;
			spsVui.sar_height = 0;
		}
		
		spsVui.flags.overscan_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (spsVui.flags.overscan_info_present_flag == 1) {
			spsVui.flags.overscan_appropriate_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		}
		else {
			spsVui.flags.overscan_appropriate_flag = 0;
		}
		
		spsVui.flags.video_signal_type_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (spsVui.flags.video_signal_type_present_flag == 1) {
			spsVui.video_format = bitstreamGetBitValue(&bitstreamBytes, &bit, 3);
			spsVui.flags.video_full_range_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			spsVui.flags.colour_description_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			if (spsVui.flags.colour_description_present_flag == 1) {
				spsVui.colour_primaries = bitstreamGetBitValue(&bitstreamBytes, &bit, 8);
				spsVui.transfer_characteristics = bitstreamGetBitValue(&bitstreamBytes, &bit, 8);
				spsVui.matrix_coeffs = bitstreamGetBitValue(&bitstreamBytes, &bit, 8);
			}
			else {
				spsVui.colour_primaries = 0;
				spsVui.transfer_characteristics = 0;
				spsVui.matrix_coeffs = 0;
			}
		}
		else {
			spsVui.video_format = 0;
			spsVui.flags.video_full_range_flag = 0;
			spsVui.flags.colour_description_present_flag = 0;
			spsVui.colour_primaries = 0;
			spsVui.transfer_characteristics = 0;
			spsVui.matrix_coeffs = 0;
		}
		
		
		spsVui.flags.chroma_loc_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (spsVui.flags.chroma_loc_info_present_flag == 1) {
			spsVui.chroma_sample_loc_type_top_field = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.chroma_sample_loc_type_bottom_field = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		}
		else {
			spsVui.chroma_sample_loc_type_top_field = 0;
			spsVui.chroma_sample_loc_type_bottom_field = 0;
		}
		
		spsVui.flags.neutral_chroma_indication_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		spsVui.flags.field_seq_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		spsVui.flags.frame_field_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		
		spsVui.flags.default_display_window_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (spsVui.flags.default_display_window_flag == 1) {
			spsVui.def_disp_win_left_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.def_disp_win_right_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.def_disp_win_top_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.def_disp_win_bottom_offset = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		}
		else {
			spsVui.def_disp_win_left_offset = 0;
			spsVui.def_disp_win_right_offset = 0;
			spsVui.def_disp_win_top_offset = 0;
			spsVui.def_disp_win_bottom_offset = 0;
		}
		
		spsVui.flags.vui_timing_info_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (spsVui.flags.vui_timing_info_present_flag == 1) {
			spsVui.vui_num_units_in_tick = bitstreamGetBitValue(&bitstreamBytes, &bit, 32);
			spsVui.vui_time_scale = bitstreamGetBitValue(&bitstreamBytes, &bit, 32);
			spsVui.flags.vui_poc_proportional_to_timing_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			if (spsVui.flags.vui_poc_proportional_to_timing_flag == 1) {
				spsVui.vui_num_ticks_poc_diff_one_minus1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			}
			else {
				spsVui.vui_num_ticks_poc_diff_one_minus1 = 0;
			}
			spsVui.flags.vui_hrd_parameters_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			if (spsVui.flags.vui_hrd_parameters_present_flag == 1) {
				return ERROR_PARSE_ISSUE; //TODO
			}
			else {
				spsVui.pHrdParameters = NULL;
			}
		}
		else {
			spsVui.vui_num_units_in_tick = 0;
			spsVui.vui_time_scale = 0;
			spsVui.flags.vui_poc_proportional_to_timing_flag = 0;
			spsVui.vui_num_ticks_poc_diff_one_minus1 = 0;
			spsVui.flags.vui_hrd_parameters_present_flag = 0;
			spsVui.pHrdParameters = NULL;
		}
		
		spsVui.flags.bitstream_restriction_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (spsVui.flags.bitstream_restriction_flag == 1) {
			spsVui.flags.tiles_fixed_structure_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			spsVui.flags.motion_vectors_over_pic_boundaries_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			spsVui.flags.restricted_ref_pic_lists_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			spsVui.min_spatial_segmentation_idc = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.max_bytes_per_pic_denom = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.max_bits_per_min_cu_denom = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.log2_max_mv_length_horizontal = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			spsVui.log2_max_mv_length_vertical = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		}
		else {
			spsVui.flags.tiles_fixed_structure_flag = 0;
			spsVui.flags.motion_vectors_over_pic_boundaries_flag = 0;
			spsVui.flags.restricted_ref_pic_lists_flag = 0;
			spsVui.min_spatial_segmentation_idc = 0;
			spsVui.max_bytes_per_pic_denom = 0;
			spsVui.max_bits_per_min_cu_denom = 0;
			spsVui.log2_max_mv_length_horizontal = 0;
			spsVui.log2_max_mv_length_vertical = 0;
		}
		
		spsVui.reserved1 = 0;
		spsVui.reserved2 = 0;
		spsVui.reserved3 = 0;
		
		sps.pSequenceParameterSetVui = &spsVui;
	}
	else {
		sps.pSequenceParameterSetVui = NULL;
	}
	
	
	sps.flags.sps_extension_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (sps.flags.sps_extension_present_flag == 1) {
		sps.flags.sps_range_extension_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //sps_multilayer_extension_flag
		if (u64 == 1) {
			return ERROR_PARSE_ISSUE;
		}
		u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //sps_3d_extension_flag
		if (u64 == 1) {
			return ERROR_PARSE_ISSUE;
		}
		sps.flags.sps_scc_extension_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 4); //sps_extension_4bits
		if (u64 > 0) {
			return ERROR_PARSE_ISSUE;
		}
	}
	else {
		sps.flags.sps_range_extension_flag = 0;
		sps.flags.sps_scc_extension_flag = 0;
	}
	
	
	if (sps.flags.sps_range_extension_flag == 1) {
		sps.flags.transform_skip_rotation_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.transform_skip_context_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.implicit_rdpcm_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.explicit_rdpcm_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.extended_precision_processing_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.intra_smoothing_disabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.high_precision_offsets_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.persistent_rice_adaptation_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.cabac_bypass_alignment_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	}
	else {
		sps.flags.transform_skip_rotation_enabled_flag = 0;
		sps.flags.transform_skip_context_enabled_flag = 0;
		sps.flags.implicit_rdpcm_enabled_flag = 0;
		sps.flags.explicit_rdpcm_enabled_flag = 0;
		sps.flags.extended_precision_processing_flag = 0;
		sps.flags.intra_smoothing_disabled_flag = 0;
		sps.flags.high_precision_offsets_enabled_flag = 0;
		sps.flags.persistent_rice_adaptation_enabled_flag = 0;
		sps.flags.cabac_bypass_alignment_enabled_flag = 0;
	}
	
  if (sps.flags.sps_scc_extension_flag == 1) {
		sps.flags.sps_curr_pic_ref_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		sps.flags.palette_mode_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (sps.flags.palette_mode_enabled_flag == 1) {
			sps.palette_max_size = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			sps.delta_palette_max_predictor_size = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			sps.flags.sps_palette_predictor_initializers_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
			if (sps.flags.sps_palette_predictor_initializers_present_flag == 1) {
				//sps.sps_num_palette_predictor_initializers_minus1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
				return ERROR_PARSE_ISSUE; //TODO
			}
			else {
				sps.sps_num_palette_predictor_initializers_minus1 = 0;
				sps.pPredictorPaletteEntries = NULL;
			}
		}
		else {
			sps.palette_max_size = 0;
			sps.delta_palette_max_predictor_size = 0;
			sps.flags.sps_palette_predictor_initializers_present_flag = 0;
			sps.sps_num_palette_predictor_initializers_minus1 = 0;
			sps.pPredictorPaletteEntries = NULL;
		}
		
		sps.motion_vector_resolution_control_idc = bitstreamGetBitValue(&bitstreamBytes, &bit, 2);
		sps.flags.intra_boundary_filtering_disabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	}
	else {
		sps.flags.sps_curr_pic_ref_enabled_flag = 0;
		sps.flags.palette_mode_enabled_flag = 0;
		sps.palette_max_size = 0;
		sps.delta_palette_max_predictor_size = 0;
		sps.flags.sps_palette_predictor_initializers_present_flag = 0;
		sps.sps_num_palette_predictor_initializers_minus1 = 0;
		sps.pPredictorPaletteEntries = NULL;
		sps.motion_vector_resolution_control_idc = 0;
		sps.flags.intra_boundary_filtering_disabled_flag = 0;
	}
	
	//SPS byte alignment
	u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //stop bit
	if (u64 != 1) {
		return ERROR_PARSE_ISSUE; //TODO
	}
	if (bit != 0x80) {
		bitstreamBytes++;
		bit = 0x80;
	}
	
	sps.reserved1 = 0;
	sps.reserved2 = 0;
	//SPS Finished Setup
	
	
	startCodeCheck = (uint64_t*) bitstreamBytes;
	if ((*startCodeCheck & 0xFFFFFFFFFFFF) != 0x014401000000) { //PPS
		return ERROR_PARSE_ISSUE;
	}
	bitstreamBytes += 6;
	
	//consoleWriteLineSlow("Got Here YES!");
	//consoleBufferFlush();
	//return ERROR_PARSE_ISSUE;
	
	
	
	pps.pps_pic_parameter_set_id = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
  pps.pps_seq_parameter_set_id = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	pps.flags.dependent_slice_segments_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.output_flag_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	pps.num_extra_slice_header_bits = bitstreamGetBitValue(&bitstreamBytes, &bit, 3);
  pps.flags.sign_data_hiding_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.cabac_init_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);	
	pps.num_ref_idx_l0_default_active_minus1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
  pps.num_ref_idx_l1_default_active_minus1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	pps.init_qp_minus26 = bitstreamGetExpGolombSignedValue(&bitstreamBytes, &bit);
  pps.flags.constrained_intra_pred_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.transform_skip_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	
  pps.flags.cu_qp_delta_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	
	if (pps.flags.cu_qp_delta_enabled_flag == 1) {
		pps.diff_cu_qp_delta_depth = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
	}
	else {
		pps.diff_cu_qp_delta_depth = 0;
	}
	
	pps.pps_cb_qp_offset = bitstreamGetExpGolombSignedValue(&bitstreamBytes, &bit);
	pps.pps_cr_qp_offset = bitstreamGetExpGolombSignedValue(&bitstreamBytes, &bit);
  pps.flags.pps_slice_chroma_qp_offsets_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.weighted_pred_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.weighted_bipred_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.transquant_bypass_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.tiles_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	pps.flags.entropy_coding_sync_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  
	if (pps.flags.tiles_enabled_flag == 1) {
		pps.num_tile_columns_minus1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		pps.num_tile_rows_minus1 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
		pps.flags.uniform_spacing_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (pps.flags.uniform_spacing_flag == 0) {
			for (uint64_t i = 0; i < pps.num_tile_columns_minus1; i++) {
				pps.column_width_minus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			}
			for (uint64_t i = 0; i < pps.num_tile_rows_minus1; i++) {
				pps.row_height_minus1[i] = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
			}
		}
		else {
			for (uint64_t i = 0; i < STD_VIDEO_H265_CHROMA_QP_OFFSET_TILE_COLS_LIST_SIZE; i++) {
				pps.column_width_minus1[i] = 0;
				pps.row_height_minus1[i] = 0;
			}
		}
		pps.flags.loop_filter_across_tiles_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	}
	else {
		pps.num_tile_columns_minus1 = 0;
		pps.num_tile_rows_minus1 = 0;
		pps.flags.uniform_spacing_flag = 0;
		for (uint64_t i = 0; i < STD_VIDEO_H265_CHROMA_QP_OFFSET_TILE_COLS_LIST_SIZE; i++) {
			pps.column_width_minus1[i] = 0;
			pps.row_height_minus1[i] = 0;
		}
		pps.flags.loop_filter_across_tiles_enabled_flag = 0;
	}
	
  pps.flags.pps_loop_filter_across_slices_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
  pps.flags.deblocking_filter_control_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);	
	if (pps.flags.deblocking_filter_control_present_flag == 1) {
		pps.flags.deblocking_filter_override_enabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		pps.flags.pps_deblocking_filter_disabled_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		if (pps.flags.pps_deblocking_filter_disabled_flag == 0) {
			pps.pps_beta_offset_div2 = bitstreamGetExpGolombSignedValue(&bitstreamBytes, &bit);
			pps.pps_tc_offset_div2 = bitstreamGetExpGolombSignedValue(&bitstreamBytes, &bit);
		}
		else {
			pps.pps_beta_offset_div2 = 0;
			pps.pps_tc_offset_div2 = 0;
		}
	}
	else {
		pps.flags.deblocking_filter_override_enabled_flag = 0;
		pps.flags.pps_deblocking_filter_disabled_flag = 0;
		pps.pps_beta_offset_div2 = 0;
		pps.pps_tc_offset_div2 = 0;
	}
	
  pps.flags.pps_scaling_list_data_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (pps.flags.pps_scaling_list_data_present_flag == 1) {
		return ERROR_PARSE_ISSUE; //TODO
	}
	else {
		 pps.pScalingLists = NULL;
	}
	
  pps.flags.lists_modification_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);	
	pps.log2_parallel_merge_level_minus2 = bitstreamGetExpGolombUnsignedValue(&bitstreamBytes, &bit);
  pps.flags.slice_segment_header_extension_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	
  pps.flags.pps_extension_present_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
	if (pps.flags.pps_extension_present_flag == 1) {
		pps.flags.pps_range_extension_flag = bitstreamGetBitValue(&bitstreamBytes, &bit, 1);
		u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //pps_multilayer_extension_flag
		if (u64 == 1) {
			return ERROR_PARSE_ISSUE;
		}
		u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //pps_3d_extension_flag
		if (u64 == 1) {
			return ERROR_PARSE_ISSUE;
		}
		t0 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //pps_scc_extension_flag ...???
		u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 4); //pps_extension_4bits
		if (u64 > 0) {
			return ERROR_PARSE_ISSUE;
		}
	}
	else {
		pps.flags.pps_range_extension_flag = 0;
		t0 = 0; //pps_scc_extension_flag
	}
	
	//consoleWriteLineWithNumberFast("Num: ", 5, *bitstreamBytes, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Bit: ", 5, bit, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Val: ", 5, pps.flags.lists_modification_present_flag, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	if (pps.flags.pps_range_extension_flag == 1) {
		return ERROR_PARSE_ISSUE; //TODO
	}
	else {
		pps.log2_max_transform_skip_block_size_minus2 = 0;
		pps.flags.cross_component_prediction_enabled_flag = 0;
		pps.flags.chroma_qp_offset_list_enabled_flag = 0;
		pps.diff_cu_chroma_qp_offset_depth = 0;
		pps.chroma_qp_offset_list_len_minus1 = 0;
		for (uint64_t i = 0; i < STD_VIDEO_H265_CHROMA_QP_OFFSET_LIST_SIZE; i++) {
			pps.cb_qp_offset_list[i] = 0;
			pps.cr_qp_offset_list[i] = 0;
		}
		pps.log2_sao_offset_scale_luma = 0;
		pps.log2_sao_offset_scale_chroma = 0;
	}
	
	if (t0 == 1) { //pps_scc_extension_flag
		return ERROR_PARSE_ISSUE; //TODO
	}
	else {
		pps.flags.pps_curr_pic_ref_enabled_flag = 0;
		pps.flags.residual_adaptive_colour_transform_enabled_flag = 0;
		pps.flags.pps_slice_act_qp_offsets_present_flag = 0;
		pps.pps_act_y_qp_offset_plus5 = 0;
		pps.pps_act_cb_qp_offset_plus5 = 0;
		pps.pps_act_cr_qp_offset_plus3 = 0;
		pps.flags.pps_palette_predictor_initializers_present_flag = 0;
		pps.pps_num_palette_predictor_initializers = 0;
		pps.flags.monochrome_palette_flag = 0;
		pps.luma_bit_depth_entry_minus8 = 0;
		pps.chroma_bit_depth_entry_minus8 = 0;
		
		pps.pPredictorPaletteEntries = NULL;
	}
  
  pps.sps_video_parameter_set_id = sps.sps_video_parameter_set_id; //Associated with the last sps
  
	//PPS byte alignment
	u64 = bitstreamGetBitValue(&bitstreamBytes, &bit, 1); //stop bit
	if (u64 != 1) {
		return ERROR_PARSE_ISSUE; //TODO
	}
	if (bit != 0x80) {
		bitstreamBytes++;
		bit = 0x80;
	}
	
  pps.reserved1 = 0;
  pps.reserved2 = 0;
  pps.reserved3 = 0;
	//PPS Final Setup
	
	*memPtr = bitstreamBytes;
	return 0;
}

static VkVideoProfileInfoKHR videoProfileInfo;
static VkVideoDecodeH265ProfileInfoKHR videoProfileH265Info;

static VkDevice device = VK_NULL_HANDLE;
static VkQueue graphicsComputeTransferQueue = VK_NULL_HANDLE;
static VkQueue videoQueue = VK_NULL_HANDLE;

int setupVulkanVideo() {
	uint32_t graphicsComputeTransferQFI = 0;
	uint32_t videoQFI = 0;
	
	videoProfileInfo.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
	
	videoProfileH265Info.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR;
	videoProfileH265Info.pNext = NULL;
	videoProfileH265Info.stdProfileIdc = 4;
	
	videoProfileInfo.pNext = &videoProfileH265Info;
	videoProfileInfo.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
	videoProfileInfo.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
	videoProfileInfo.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
	videoProfileInfo.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
	
	VkVideoCapabilitiesKHR videoCapabilities;
	videoCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
	
	VkVideoDecodeCapabilitiesKHR videoDecodeCapabilities;
	videoDecodeCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
	
	VkVideoDecodeH265CapabilitiesKHR videoDecodeH265Capabilities;
	videoDecodeH265Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;
	videoDecodeH265Capabilities.pNext = NULL;
	
	videoDecodeCapabilities.pNext = &videoDecodeH265Capabilities;
	videoCapabilities.pNext = &videoDecodeCapabilities;
	
	uint32_t formatCount = 32;
	
	void* memPage = NULL;
	uint64_t memPageBytes = 0;
	int error = memoryAllocateOnePage(&memPage, &memPageBytes);
	RETURN_ON_ERROR(error);
	
	VkVideoFormatPropertiesKHR* videoFormatProps = (VkVideoFormatPropertiesKHR*) memPage;
	for (uint32_t i = 0; i < formatCount; i++) {
		videoFormatProps[i].sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
		videoFormatProps[i].pNext = NULL;
	}
	
	error = vulkanVideoSetup(&device, &graphicsComputeTransferQFI, &videoQFI, &videoProfileInfo, &videoCapabilities, &formatCount, videoFormatProps);
	RETURN_ON_ERROR(error);
	
	vkGetDeviceQueue(device, graphicsComputeTransferQFI, 0, &graphicsComputeTransferQueue);
	vkGetDeviceQueue(device, videoQFI, 0, &videoQueue);
	
	if ((videoDecodeCapabilities.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR) == 0 ) {
		return ERROR_VULKAN_TBD;
	}
	
	uint32_t videoFormatChosen = 256;
	for (uint32_t f = 0; f < formatCount; f++) {
		//consoleWriteLineWithNumberFast("Format: ", 8, videoFormatProps[f].format, NUM_FORMAT_UNSIGNED_INTEGER);
		if (videoFormatProps[f].format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16) {
			videoFormatChosen = f;
			break;
		}
	}
	if (videoFormatChosen == 256) {
		return ERROR_VULKAN_TBD;
	}
	
	
	error = memoryDeallocate(&memPage);
	RETURN_ON_ERROR(error);
	
	return 0;
}




//Program Main Function
int programMain() {
	int error = 0;
	
	//Open Bitstream File and Extract the Dimensions
	consolePrintLine(54);
	void* h265File = NULL;
	char* inputFileName = "bitstream.h265";
	error = ioOpenFile(&h265File, inputFileName, -1, IO_FILE_READ_NORMAL);
	RETURN_ON_ERROR(error);
	
	//16MB Allocate:
	void* memAlloc = NULL;
	uint64_t memAllocBytes = 4096 * 4096;
	error = memoryAllocate(&memAlloc, memAllocBytes, 0);
	RETURN_ON_ERROR(error);
	uint8_t* memPtr = (uint8_t*) memAlloc;
	
	uint64_t* nalReservedHeader = (uint64_t*) memPtr;
	uint32_t* nalReservedSize = (uint32_t*) (&(memPtr[6]));
	
	uint32_t bytesRead = 10;
	error = ioReadFile(h265File, memAlloc, &bytesRead);
	RETURN_ON_ERROR(error);
	if (bytesRead != 10) {
		return 1;
	}
	if ((*nalReservedHeader & 0xFFFFFFFFFFFF) != 0x015401000000) {
		return 2;
	}
	
	bytesRead = *nalReservedSize;
	if (*nalReservedSize > memAllocBytes) {
		//return 3;
		bytesRead = memAllocBytes;
	}
	
	error = ioReadFile(h265File, memAlloc, &bytesRead);
	RETURN_ON_ERROR(error);
	
	int errorMinor = readBitstreamParameters(&memPtr);
	if (errorMinor != 0) {
		error = memoryDeallocate(&memAlloc);
		RETURN_ON_ERROR(error);
		error = ioCloseFile(&h265File);
		RETURN_ON_ERROR(error);
		return errorMinor;
	}
	uint64_t paramBytes = memPtr - ((uint8_t*) (memAlloc));
	uint64_t sliceOffset = paramBytes + 10;
	uint64_t sliceBytes = (*nalReservedSize) - paramBytes;
	
	uint64_t* startCodeCheck = (uint64_t*) memPtr;
	if ((*startCodeCheck & 0xFFFFFFFFFFFF) != 0x012601000000) { //IDR Slice
		consoleWriteLineSlow("NO IDR!");
		consoleBufferFlush();
		return ERROR_PARSE_ISSUE;
	}
	
	error = setupVulkanVideo();
	RETURN_ON_ERROR(error);	
	
	consolePrintLine(52);
	
	/*
	void* yuvFile = NULL;
	inputFileName = "idr.yuv";
	error = ioOpenFile(&yuvFile, inputFileName, -1, IO_FILE_WRITE_NORMAL);
	RETURN_ON_ERROR(error);
	
	//Close File
	error = ioCloseFile(&yuvFile);
	RETURN_ON_ERROR(error);
	//*/
	
	error = ioCloseFile(&h265File);
	RETURN_ON_ERROR(error);
	
	//Cleanup ALL Vulkan Elements
	
	
	return 0; //Exit Program Successfully
}

