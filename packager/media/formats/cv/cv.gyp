# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
    '../../../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'cv',
      'type': '<(component)',
      'sources': [
        'cv_media_parser.cc',
        'cv_media_parser.h',
      ],
      'dependencies': [
        '../../../third_party/boringssl/boringssl.gyp:boringssl',
        '../../base/media_base.gyp:media_base',
        '../../codecs/codecs.gyp:codecs',
        '../../event/media_event.gyp:media_event',
      ],
    },
    {
      'target_name': 'cv_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'cv_media_parser_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../file/file.gyp:file',
        '../../test/media_test.gyp:media_test_support',
        'cv',
      ]
    },
  ],
}
