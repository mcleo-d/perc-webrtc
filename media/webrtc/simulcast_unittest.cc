/*
 * libjingle
 * Copyright 2014 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/media/webrtc/simulcast.h"

namespace cricket {

class ScreenshareLayerConfigTest : public testing::Test,
                                   protected ScreenshareLayerConfig {
 public:
  ScreenshareLayerConfigTest() : ScreenshareLayerConfig(0, 0) {}

  void ExpectParsingFails(const std::string& group) {
    ScreenshareLayerConfig config(100, 1000);
    EXPECT_FALSE(FromFieldTrialGroup(group, &config));
  }
};

TEST_F(ScreenshareLayerConfigTest, UsesDefaultBitrateConfigForDefaultGroup) {
  ExpectParsingFails("");
}

TEST_F(ScreenshareLayerConfigTest, UsesDefaultConfigForInvalidBitrates) {
  ExpectParsingFails("-");
  ExpectParsingFails("1-");
  ExpectParsingFails("-1");
  ExpectParsingFails("-12");
  ExpectParsingFails("12-");
  ExpectParsingFails("booh!");
  ExpectParsingFails("1-b");
  ExpectParsingFails("a-2");
  ExpectParsingFails("49-1000");
  ExpectParsingFails("50-6001");
  ExpectParsingFails("100-99");
  ExpectParsingFails("1002003004005006-99");
  ExpectParsingFails("99-1002003004005006");
}

TEST_F(ScreenshareLayerConfigTest, ParsesValidBitrateConfig) {
  ScreenshareLayerConfig config(100, 1000);
  EXPECT_TRUE(ScreenshareLayerConfig::FromFieldTrialGroup("101-1001", &config));
  EXPECT_EQ(101, config.tl0_bitrate_kbps);
  EXPECT_EQ(1001, config.tl1_bitrate_kbps);
}

}  // namespace cricket
