/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_H_
#define WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_H_

#include <string>
#include <vector>

#include "webrtc/api/call/transport.h"
#include "webrtc/config.h"

namespace webrtc {

class FlexfecReceiveStream {
 public:
  struct Stats {
    std::string ToString(int64_t time_ms) const;

    // TODO(brandtr): Add appropriate stats here.
    int flexfec_bitrate_bps;
  };

  struct Config {
    std::string ToString() const;

    // Payload type for FlexFEC.
    int payload_type = -1;

    // SSRC for FlexFEC stream to be received.
    uint32_t remote_ssrc = 0;

    // Vector containing a single element, corresponding to the SSRC of the
    // media stream being protected by this FlexFEC stream. The vector MUST have
    // size 1.
    //
    // TODO(brandtr): Update comment above when we support multistream
    // protection.
    std::vector<uint32_t> protected_media_ssrcs;

    // SSRC for RTCP reports to be sent.
    uint32_t local_ssrc = 0;

    // What RTCP mode to use in the reports.
    RtcpMode rtcp_mode = RtcpMode::kCompound;

    // Transport for outgoing RTCP packets.
    Transport* rtcp_send_transport = nullptr;

    // |transport_cc| is true whenever the send-side BWE RTCP feedback message
    // has been negotiated. This is a prerequisite for enabling send-side BWE.
    bool transport_cc = false;

    // RTP header extensions that have been negotiated for this track.
    std::vector<RtpExtension> extensions;
  };

  // Starts stream activity.
  // When a stream is active, it can receive and process packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive nor process packets.
  virtual void Stop() = 0;

  virtual Stats GetStats() const = 0;

 protected:
  virtual ~FlexfecReceiveStream() = default;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_H_
