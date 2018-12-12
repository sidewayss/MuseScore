//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2017 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#ifndef __PAGESETTINGS_H__
#define __PAGESETTINGS_H__

#include "ui_pagesettings.h"
#include "abstractdialog.h"

namespace Ms {

class MasterScore;
class Score;
class Navigator;

//---------------------------------------------------------
//   PageSettings
//---------------------------------------------------------

class PageSettings : public AbstractDialog, private Ui::PageSettingsBase {
      Q_OBJECT

      Navigator*   preview;
      Score* cs;
      Score* clonedScore;

      const double picaCicero = 12;
      const double didotToPt  =  1.06574601373228;

//      std::unique_ptr<Score> clonedScoreForNavigator;

      virtual void hideEvent(QHideEvent*);
      void updateWidgets();
      void updateWidthHeight(QRectF & rect);
      void updatePreview();
      void blockSignals(bool);
      void applyToScore(Score*);
      void setPageSize(QPageSize::PageSizeId psid);
      void marginMinMax(double val, double max, QPageLayout* layout, QDoubleSpinBox* spinner);
      void lrMargins(double val, bool isL, bool isOdd, QPageLayout* one, QPageLayout* other, QDoubleSpinBox* spinOne);

   private slots:
      void sizeChanged(int);
      void unitsChanged();

      void resetToDefault();
      void apply();
      void applyToAllParts();
      void ok();
      void done(int val);
      
      void twosidedToggled(bool b);
      void orientationToggled(bool);
      void otmChanged(double val);
      void obmChanged(double val);
      void olmChanged(double val);
      void ormChanged(double val);
      void etmChanged(double val);
      void ebmChanged(double val);
      void elmChanged(double val);
      void ermChanged(double val);
      void spatiumChanged(double val);
      void widthHeightChanged(double);
      void pageOffsetChanged(int val);

   protected:
      virtual void retranslate() { retranslateUi(this); }

   public:
      PageSettings(QWidget* parent = 0);
      ~PageSettings();
      void setScore(Score*);
      };
} // namespace Ms
#endif

