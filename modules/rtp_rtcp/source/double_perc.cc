/*
 *  Copyright 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/double_perc.h"

#include <string.h>

#include "third_party/libsrtp/include/srtp.h"
#include "webrtc/modules/rtp_rtcp/source/double_perc.h"
#include "webrtc/base/sslstreamadapter.h"

inline char PC(uint8_t b)
{
	if (b>32&&b<128)
		return b;
	else
		return '.';
}
inline void Debug(const char *msg, ...)
{
		va_list ap;
		va_start(ap, msg);
		vprintf(msg, ap);
		va_end(ap);
		fflush(stdout);
}

inline void Dump(const uint8_t *data,uint32_t size)
{
	for(uint32_t i=0;i<(size/8);i++)
		Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   %c%c%c%c%c%c%c%c]\n",8*i,data[8*i],data[8*i+1],data[8*i+2],data[8*i+3],data[8*i+4],data[8*i+5],data[8*i+6],data[8*i+7],PC(data[8*i]),PC(data[8*i+1]),PC(data[8*i+2]),PC(data[8*i+3]),PC(data[8*i+4]),PC(data[8*i+5]),PC(data[8*i+6]),PC(data[8*i+7]));
	switch(size%8)
	{
		case 1:
			Debug("[%.4x] [0x%.2x                                                    %c       ]\n",size-1,data[size-1],PC(data[size-1]));
			break;
		case 2:
			Debug("[%.4x] [0x%.2x   0x%.2x                                             %c%c      ]\n",size-2,data[size-2],data[size-1],PC(data[size-2]),PC(data[size-1]));
			break;
		case 3:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x                                      %c%c%c     ]\n",size-3,data[size-3],data[size-2],data[size-1],PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 4:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x                               %c%c%c%c    ]\n",size-4,data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 5:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x                        %c%c%c%c%c   ]\n",size-5,data[size-5],data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-5]),PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 6:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x                 %c%c%c%c%c%c  ]\n",size-6,data[size-6],data[size-5],data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-6]),PC(data[size-5]),PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
		case 7:
			Debug("[%.4x] [0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x   0x%.2x          %c%c%c%c%c%c%c ]\n",size-7,data[size-7],data[size-6],data[size-5],data[size-4],data[size-3],data[size-2],data[size-1],PC(data[size-7]),PC(data[size-6]),PC(data[size-5]),PC(data[size-4]),PC(data[size-3]),PC(data[size-2]),PC(data[size-1]));
			break;
	}
}


/* OHB data
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |M|     PT      |       sequence number         |  timestamp    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                  timestamp                    |  SSRC         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                  SSRC(cont                    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
static size_t ohb_size = 11;

namespace webrtc {
  
DoublePERC::DoublePERC()
    : session_(nullptr),
      rtp_auth_tag_len_(0),
      rtcp_auth_tag_len_(0) {
}

DoublePERC::~DoublePERC() {
  if (session_) {
    srtp_dealloc(session_);
  }
}
bool DoublePERC::SetOutboundKey(int cs, const uint8_t* key, size_t len) {
  return SetKey(ssrc_any_inbound, cs, key, len);
}

bool DoublePERC::SetInboundKey(int cs, const uint8_t* key, size_t len) {
  return SetKey(ssrc_any_outbound, cs, key, len);
}

bool DoublePERC::SetKey(int type, int cs, const uint8_t* key, size_t len) {

  if (session_) {
    LOG(LS_ERROR) << "Failed to create SRTP session: "
                  << "SRTP session already created";
    return false;
  }

  srtp_policy_t policy;
  memset(&policy, 0, sizeof(policy));
  if (cs == rtc::SRTP_AES128_CM_SHA1_80) {
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  } else if (cs == rtc::SRTP_AES128_CM_SHA1_32) {
    // RTP HMAC is shortened to 32 bits, but RTCP remains 80 bits.
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  } else if (cs == rtc::SRTP_AEAD_AES_128_GCM ) {
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
  } else if (cs == rtc::SRTP_AEAD_AES_256_GCM ) {
    srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
  } else {
    LOG(LS_WARNING) << "Failed to create SRTP session: unsupported"
                    << " cipher_suite " << cs;
    return false;
  }

  int expected_key_len;
  int expected_salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(cs, &expected_key_len,
      &expected_salt_len)) {
    // This should never happen.
    LOG(LS_WARNING) << "Failed to create SRTP session: unsupported"
                    << " cipher_suite without length information" << cs;
    return false;
  }

  if (!key ||
      len != static_cast<size_t>(expected_key_len + expected_salt_len)) {
    LOG(LS_WARNING) << "Failed to create SRTP session: invalid key";
    return false;
  }

  policy.ssrc.type = static_cast<srtp_ssrc_type_t>(type);
  policy.ssrc.value = 0;
  policy.key = const_cast<uint8_t*>(key);
  // TODO(astor) parse window size from WSH session-param
  policy.window_size = 1024;
  policy.allow_repeat_tx = 1;

  policy.next = nullptr;

  int err = srtp_create(&session_, &policy);
  if (err != srtp_err_status_ok) {
    session_ = nullptr;
    LOG(LS_ERROR) << "Failed to create SRTP session, err=" << err;
    return false;
  }

  rtp_auth_tag_len_ = policy.rtp.auth_tag_len;
  rtcp_auth_tag_len_ = policy.rtcp.auth_tag_len;
  return true;
}

bool DoublePERC::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  if (!session_) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet: no SRTP Session";
    return false;
  }

  int need_len = in_len + rtp_auth_tag_len_;  // NOLINT
  if (max_len < need_len) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet: The buffer length "
                    << max_len << " is less than the needed " << need_len;
    return false;
  }

  *out_len = in_len;
  int err = srtp_protect(session_, p, out_len);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to encrypt double packet";
    return false;
  }
  return true;
}


bool DoublePERC::UnprotectRtp(void* p, int in_len, int* out_len) {
  
  if (!session_) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet: no SRTP Session";
    return false;
  }

  *out_len = in_len;
  int err = srtp_unprotect(session_, p, out_len);

  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet, err=" << err;
    return false;
  }
  return true;
}

size_t DoublePERC::GetEncryptionOverhead()
{
	return ohb_size + rtp_auth_tag_len_;
}

bool DoublePERC::Encrypt(rtp::Packet *packet)
{
  // Calculate payload size for encrypted version
  size_t encrypted_payload_size = ohb_size + packet->payload_size() + rtp_auth_tag_len_;
  
  //Check it is enought
  if (encrypted_payload_size > packet->MaxPayloadSize()) {
    LOG(LS_WARNING) << "Failed to perform DOUBLE PERC"
      << " encrypted size will exceed max payload size available";
    return false;
  }
  // Alloc temporal buffer for encryption
  size_t size = encrypted_payload_size + 1;
  uint8_t* inner = (uint8_t*) malloc(size);
  
  //Get packet values
  bool mark = packet->Marker ();
  uint8_t pt = packet->PayloadType ();
  uint16_t seq = packet->SequenceNumber();
  uint32_t ts = packet->Timestamp();
  uint32_t ssrc = packet->Ssrc();
  
  // Innert RTP packet has no padding,csrcs or extensions
  inner[0] = 0x80;
  
  // marker & pt
  inner[1] = mark ? 0x80 | pt : pt;
  //SEQ
  inner[2] = seq >> 8;
  inner[3] = seq;
  // TS
  inner[4] = ts >> 24;
  inner[5] = ts >> 16;
  inner[6] = ts >> 8;
  inner[7] = ts;
  // SSRC
  inner[8] = ssrc >> 24;
  inner[9] = ssrc >> 16;
  inner[10] = ssrc >> 8;
  inner[11] = ssrc;
  
  //Copy the rest of the payload
  memcpy(inner + 1 + ohb_size, packet->payload().data(), packet->payload_size());

  LOG(LS_WARNING) << ">Encrypt "  << packet->payload_size()
    << " seq " << packet->SequenceNumber () 
    << " ts  " << packet->Timestamp () 
    << " ssrc" << packet->Ssrc ();
  Dump(packet->payload().data(),16);
  Dump(inner,32);
  // Protect inner rtp packet
  int out_len;
  bool result = ProtectRtp(inner,
                           1 + ohb_size + packet->payload_size(),
                           size,
                           &out_len);
  LOG(LS_WARNING) << "<Encrypt " << out_len 
    << " seq " << (((uint32_t)(inner[2]))<<8 | inner[3])
    << " ts  " << (((uint32_t)(inner[4]))<<24 | ((uint32_t)(inner[5]))<<16 | ((uint32_t)(inner[6]))<<8 | ((uint32_t)(inner[7])))
    << " ssrc" << (((uint32_t)(inner[8]))<<24 | ((uint32_t)(inner[9]))<<16 | ((uint32_t)(inner[10]))<<8 | ((uint32_t)(inner[11])));
  Dump(inner,32);
  
  //Set encrypted payload
  if (result) {
    // Allocate new payload size
    uint8_t* buffer = packet->AllocatePayload(out_len - 1);
    
    if (buffer) {
      // Copy the encrypted inner rtp packet except first byte  of rtp header
      memcpy(buffer, inner + 1, out_len - 1);
      // Set new payload size
      packet->SetPayloadSize(out_len - 1);
    } else {
        LOG(LS_WARNING) << "Failed to perform DOUBLE PERC"
          << " could not allocate payload for encrypted data";
        result = false;
    }
  }
  Dump(packet->payload().data(),16);
  //Free aux
  free(inner);
  
  return result;
}

bool DoublePERC::Decrypt(uint8_t* payload,size_t* payload_length) {
  //Check we have enought data on payload
  if (*payload_length < ohb_size + rtp_auth_tag_len_) {
    LOG(LS_WARNING) << "Failed to perform DOUBLE PERC"
      << " encrypted payload is smaller than the minimum possible";
    return false;
  }
  
  // Alloc temporal buffer for decryption
  uint8_t* inner = (uint8_t*) malloc(*payload_length + 1);
  
   // Reconstruct RTP header
  inner[0] = 0x80;
  // Copy the rest of the header
  memcpy(inner + 1, payload, *payload_length);
  
  LOG(LS_WARNING) << ">Decrypt " << *payload_length
    << " seq " << (((uint32_t)(inner[2]))<<8 | inner[3])
    << " ts  " << (((uint32_t)(inner[4]))<<24 | ((uint32_t)(inner[5]))<<16 | ((uint32_t)(inner[6]))<<8 | ((uint32_t)(inner[7])))
    << " ssrc" << (((uint32_t)(inner[8]))<<24 | ((uint32_t)(inner[9]))<<16 | ((uint32_t)(inner[10]))<<8 | ((uint32_t)(inner[11])));
  Dump(payload,16);
  Dump(inner,16);
  // UnProtect inner rtp packet
  int out_length;
  bool result = UnprotectRtp(inner,
                           1 + *payload_length,
                           &out_length);
  
   LOG(LS_WARNING) << "<Decrypt" << out_length;
 //Set decyrpted payload
  if (result) {
    // Remove the OHB data
    *payload_length = out_length - ohb_size - 1;
    // Copy the encrypted inner rtp packet except first byte  of rtp header
    memcpy(payload, inner + ohb_size + 1, *payload_length);
  } else {
      LOG(LS_WARNING) << "Failed to perform DOUBLE PERC";
      result = false;
  }
  Dump(inner,16);
  Dump(payload,16);
 
  //Free aux
  free(inner);
  
  return result;
}

}