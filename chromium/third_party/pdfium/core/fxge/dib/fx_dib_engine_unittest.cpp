// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/render/render_int.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxge/dib/dib_int.h"
#include "core/fxge/fx_dib.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/base/ptr_util.h"

TEST(CStretchEngine, OverflowInCtor) {
  FX_RECT clip_rect;
  std::unique_ptr<CPDF_Dictionary> dict_obj =
      pdfium::MakeUnique<CPDF_Dictionary>();
  dict_obj->SetFor("Width", new CPDF_Number(71000));
  dict_obj->SetFor("Height", new CPDF_Number(12500));
  std::unique_ptr<CPDF_Stream> stream =
      pdfium::MakeUnique<CPDF_Stream>(nullptr, 0, dict_obj.release());
  CPDF_DIBSource dib_source;
  dib_source.Load(nullptr, stream.get(), nullptr, nullptr, nullptr, nullptr,
                  false, 0, false);
  CStretchEngine engine(nullptr, FXDIB_8bppRgb, 500, 500, clip_rect,
                        &dib_source, 0);
  EXPECT_EQ(FXDIB_INTERPOL, engine.m_Flags);
}
