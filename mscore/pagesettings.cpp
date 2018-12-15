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

#include "globals.h"
#include "pagesettings.h"
#include "preferences.h"
#include "libmscore/page.h"
#include "libmscore/style.h"
#include "libmscore/score.h"
#include "navigator.h"
#include "libmscore/mscore.h"
#include "libmscore/excerpt.h"
#include "musescore.h"

namespace Ms {

//---------------------------------------------------------
//   PageSettings
//---------------------------------------------------------

PageSettings::PageSettings(QWidget* parent)
   : AbstractDialog(parent)
      {
      clonedScore = 0;
      setObjectName("PageSettings");
      setupUi(this);
      setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
      setModal(true);

      NScrollArea* sa = new NScrollArea;
      preview = new Navigator(sa, this);
//      preview->setPreviewOnly(true);

      static_cast<QVBoxLayout*>(previewGroup->layout())->insertWidget(0, sa);

      MuseScore::restoreGeometry(this);
      int i;
      QString name;
      for (i = 0; i < sizeCount; ++i) // omit postcards and envelopes
            sizesList->addItem(QPageSize::name(QPageSize::PageSizeId(i)));

	for (i = 0; i <= QPageSize::Cicero; ++i)
	      unitsList->addItem(QString("%1 (%2)").arg(units[i].name())
                                                 .arg(units[i].suffix()));
      unitsGroup->setVisible(!preferences.getBool(PREF_APP_PAGE_UNITS_GLOBAL));      
      
      ///!!!widget naming is inconsistent across modules in MuseScore.
      ///!!!I am following a mix of conventions and leaving some names unchanged
      connect(buttonReset,          SIGNAL(clicked()),            SLOT(resetToDefault()));
      connect(buttonApplyToAllParts,SIGNAL(clicked()),            SLOT(applyToAllParts()));
      connect(buttonApply,          SIGNAL(clicked()),            SLOT(apply()));
      connect(buttonOk,             SIGNAL(clicked()),            SLOT(ok()));
      connect(portraitButton,       SIGNAL(toggled(bool)),        SLOT(orientationToggled(bool)));
      connect(landscapeButton,      SIGNAL(toggled(bool)),        SLOT(orientationToggled(bool)));
      connect(twosided,             SIGNAL(toggled(bool)),        SLOT(twosidedToggled(bool)));
      ///!!!currentIndexChanged only fires when the index changes, activated always fires
      connect(sizesList,            SIGNAL(currentIndexChanged(int)), SLOT(sizeChanged(int)));
      connect(unitsList,            SIGNAL(currentIndexChanged(int)), SLOT(unitsChanged()));
      connect(pageWidth,            SIGNAL(valueChanged(double)), SLOT(widthChanged(double)));
      connect(pageHeight,           SIGNAL(valueChanged(double)), SLOT(heightChanged(double)));
      connect(spatiumEntry,         SIGNAL(valueChanged(double)), SLOT(spatiumChanged(double)));
	connect(oddPageTopMargin,     SIGNAL(valueChanged(double)), SLOT(otmChanged(double)));
      connect(oddPageBottomMargin,  SIGNAL(valueChanged(double)), SLOT(obmChanged(double)));
      connect(oddPageLeftMargin,    SIGNAL(valueChanged(double)), SLOT(olmChanged(double)));
      connect(oddPageRightMargin,   SIGNAL(valueChanged(double)), SLOT(ormChanged(double)));
      connect(evenPageTopMargin,    SIGNAL(valueChanged(double)), SLOT(etmChanged(double)));
      connect(evenPageBottomMargin, SIGNAL(valueChanged(double)), SLOT(ebmChanged(double)));
      connect(evenPageRightMargin,  SIGNAL(valueChanged(double)), SLOT(ermChanged(double)));
      connect(evenPageLeftMargin,   SIGNAL(valueChanged(double)), SLOT(elmChanged(double)));
      connect(pageOffsetEntry,      SIGNAL(valueChanged(int)),    SLOT(pageOffsetChanged(int)));
      }

//---------------------------------------------------------
//   PageSettings
//---------------------------------------------------------

PageSettings::~PageSettings()
      {
      delete clonedScore;
      }

//---------------------------------------------------------
//   hideEvent
//---------------------------------------------------------
///!!!what does this function do? when is it called???
void PageSettings::hideEvent(QHideEvent* ev)
      {
      MuseScore::saveGeometry(this);
      QWidget::hideEvent(ev);
      }

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void PageSettings::setScore(Score* s)
      {
      cs = s;
      delete clonedScore;
      clonedScore = s->clone();
      clonedScore->setLayoutMode(LayoutMode::PAGE);

      clonedScore->doLayout();
      preview->setScore(clonedScore);
      buttonApplyToAllParts->setEnabled(!cs->isMaster());
      updateWidgets();
      updatePreview();
      }

//---------------------------------------------------------
//   blockSignals - helper for updateWidgets
//---------------------------------------------------------

void PageSettings::blockSignals(bool block)
      {
      for (auto w : { oddPageTopMargin, oddPageBottomMargin, oddPageLeftMargin, oddPageRightMargin,
                     evenPageTopMargin,evenPageBottomMargin,evenPageLeftMargin,evenPageRightMargin,
                     spatiumEntry } )
            {
            w->blockSignals(block);
            }
      sizesList->blockSignals(block);
      unitsList->blockSignals(block);
      portraitButton->blockSignals(block);
      landscapeButton->blockSignals(block);
      pageOffsetEntry->blockSignals(block);
}

//---------------------------------------------------------
//   updateWidgets
//    set widget values from preview->score()
//---------------------------------------------------------

void PageSettings::updateWidgets()
      {
      blockSignals(true);
      Score*       score   = preview->score();
      QPageLayout* qpl     = score->style().pageOdd();
      int          idxUnit = int(qpl->units());
      PageUnits    pu      = units[idxUnit];
      const char*  suffix  = pu.suffix();
      double       step    = pu.step();

      QPageSize::PageSizeId psid = score->style().pageSize()->id();
      unitsList->setCurrentIndex(idxUnit);
      sizesList->setCurrentIndex(int(psid));
      spatiumEntry->setSingleStep(pu.stepSpatium());
      spatiumEntry->setSuffix(suffix);
      for (auto w : { oddPageTopMargin, oddPageBottomMargin, oddPageLeftMargin, oddPageRightMargin,
                     evenPageTopMargin,evenPageBottomMargin,evenPageLeftMargin,evenPageRightMargin,
                     pageWidth, pageHeight } )
            {
            w->setSuffix(suffix);
            w->setSingleStep(step);
            }
      ///!!!Minimum margins would be nice here, taken from the printer, maybe in main()...
      ///!!!Max margins now handled by QPageLayout in change event, obsoleting setMarginsMax

      QMarginsF marg = qpl->margins();
      oddPageTopMargin    ->setValue(marg.top());
      oddPageBottomMargin ->setValue(marg.bottom());
      oddPageLeftMargin   ->setValue(marg.left());
      oddPageRightMargin  ->setValue(marg.right());

      marg = score->style().pageEven()->margins();
      evenPageTopMargin   ->setValue(marg.top());
      evenPageBottomMargin->setValue(marg.bottom());
      evenPageLeftMargin  ->setValue(marg.left());
      evenPageRightMargin ->setValue(marg.right());

      ///!!!Qt page and printer classes round to 2 decimals - spatium needs full resolution conversion
      switch (QPageSize::Unit(idxUnit)) {
            case QPageLayout::Millimeter:
                  spatiumEntry->setValue(score->spatium() / DPI_F / PPI * INCH);
                  break;
            case QPageLayout::Inch:
                  spatiumEntry->setValue(score->spatium() / DPI_F / PPI);
                  break;
            case QPageLayout::Point:
                  spatiumEntry->setValue(score->spatium() / DPI_F);
                  break;
            case QPageLayout::Pica:
                  spatiumEntry->setValue(score->spatium() / DPI_F / picaCicero);
                  break;
            case QPageLayout::Didot:
                  spatiumEntry->setValue(score->spatium() / DPI_F / didotToPt);
                  break;
            case QPageLayout::Cicero:
                  spatiumEntry->setValue(score->spatium() / DPI_F / picaCicero / didotToPt);
                  break;
      }

      if (qpl->orientation() == QPageLayout::Portrait)
            portraitButton ->setChecked(true);
      else
            landscapeButton->setChecked(true);

      pageOffsetEntry->setValue(score->pageNumberOffset() + 1);
      blockSignals(false);

      // updateWidthHeight handles blockSignals for these widgets
      QRectF rect = qpl->fullRect();
      updateWidthHeight(rect);
      
      // twosided is left out of blockSignals, twosidedToggled() will run
      bool is2 = score->styleB(Sid::pageTwosided);
      twosided->setChecked(is2);
}

//---------------------------------------------------------
//   updateWidthHeight - updates Width and Height widgets
//---------------------------------------------------------

void PageSettings::updateWidthHeight(QRectF& rect)
      {
      pageWidth ->blockSignals(true);
      pageHeight->blockSignals(true);
      pageWidth ->setValue(rect.width());
      pageHeight->setValue(rect.height());
      pageWidth ->blockSignals(false);
      pageHeight->blockSignals(false);
      }

//---------------------------------------------------------
//   orientationToggled
//    a pair of radio buttons that toggle the page orientation
//    it swaps width and height values, but not margins.
//---------------------------------------------------------
void PageSettings::orientationToggled(bool)
      {
      QPageLayout::Orientation orient = portraitButton->isChecked()
                                      ? QPageLayout::Portrait
                                      : QPageLayout::Landscape;
      Score* score = preview->score();
      score->style().pageOdd() ->setOrientation(orient);
      score->style().pageEven()->setOrientation(orient);

      QRectF rect = score->style().pageOdd()->fullRect();
      updateWidthHeight(rect);
      updatePreview();
      }

//---------------------------------------------------------
//   twosidedToggled
//---------------------------------------------------------

void PageSettings::twosidedToggled(bool flag)
      {
      preview->score()->style().set(Sid::pageTwosided, flag);

      for (auto w : { evenPageTopMargin, evenPageBottomMargin, evenPageLeftMargin, evenPageRightMargin })
            w->setEnabled(flag);

      evenPageLeftMargin ->blockSignals(true);
      evenPageRightMargin->blockSignals(true);
      if (flag) { 
            evenPageLeftMargin ->setValue(oddPageRightMargin ->value());
            evenPageRightMargin->setValue(oddPageLeftMargin->value());
      }
      else {
            evenPageLeftMargin ->setValue(oddPageLeftMargin ->value());
            evenPageRightMargin->setValue(oddPageRightMargin->value());
      }
      evenPageLeftMargin ->blockSignals(false);
      evenPageRightMargin->blockSignals(false);

      updatePreview();
      }

//---------------------------------------------------------
//   resetToDefault
//---------------------------------------------------------

void PageSettings::resetToDefault()
      {
      
      }

//---------------------------------------------------------
//   apply
//---------------------------------------------------------

void PageSettings::apply()
      {
      applyToScore(cs);
      mscore->endCmd();
      }

//---------------------------------------------------------
//   applyToScore
//---------------------------------------------------------

void PageSettings::applyToScore(Score* s)
      {
      s->startCmd();

      Score*       ps    = preview->score();
      QPageSize*   psize = ps->style().pageSize();
      QPageLayout* odd   = ps->style().pageOdd();
      QPageLayout* even  = ps->style().pageEven();
      s->style().setPageOdd( odd ); ///!!!these 3 need undo functions...
      s->style().setPageEven(even);
      s->style().setPageSize(psize);

      s->setPageNumberOffset(ps->pageNumberOffset()); ///!!!why isn't this a style???

      s->undoChangeStyleVal(Sid::spatium,      ps->spatium());
      s->undoChangeStyleVal(Sid::pageTwosided, twosided->isChecked());

      s->style().set(Sid::pageSize,  int(psize->id())); ///!!!undo not working for new styles???
      s->style().set(Sid::pageUnits, int(odd->units()));

      QRectF rect = odd->fullRect(QPageLayout::Point);
      s->undoChangeStyleVal(Sid::pageWidth,  rect.width());
      s->undoChangeStyleVal(Sid::pageHeight, rect.height());

      QMarginsF marg = odd->margins(QPageLayout::Point);
      s->undoChangeStyleVal(Sid::pageOddLeftMargin,    marg.left());
      s->undoChangeStyleVal(Sid::pageOddRightMargin,   marg.right());
      s->undoChangeStyleVal(Sid::pageOddTopMargin,     marg.top());
      s->undoChangeStyleVal(Sid::pageOddBottomMargin,  marg.bottom());

      marg = even->margins(QPageLayout::Point);
      s->undoChangeStyleVal(Sid::pageEvenTopMargin,    marg.top());
      s->undoChangeStyleVal(Sid::pageEvenBottomMargin, marg.bottom());

///!!!      s->undoChangeStyleVal(Sid::pageEvenLeftMargin,   marg.left());
///!!!      s->undoChangeStyleVal(Sid::pagePrintableWidth, odd->paintRect(QPageLayout::Point).width());

      s->endCmd();
      }

//---------------------------------------------------------
//   applyToAllParts
//---------------------------------------------------------

void PageSettings::applyToAllParts()
      {
      for (Excerpt* e : cs->excerpts())
            applyToScore(e->partScore());
      }

//---------------------------------------------------------
//   ok
//---------------------------------------------------------

void PageSettings::ok()
      {
      apply();
      done(0);
      }

//---------------------------------------------------------
//   done
//---------------------------------------------------------

void PageSettings::done(int val)
      {
      cs->setLayoutAll();     // HACK
      QDialog::done(val);
      }

//---------------------------------------------------------
//   setPageSize - a helper for page size, width, height changes
//---------------------------------------------------------

void PageSettings::setPageSize(QPageSize::PageSizeId psid) {
      MStyle&         style = preview->score()->style();
      QPageSize::Unit unit  = QPageSize::Unit(style.pageOdd()->units());
      QPageSize*      qps;

      bool isPreset = (psid != QPageSize::Custom);
      if (isPreset)
            qps = new QPageSize(psid);
      else
            qps = new QPageSize(QSizeF(pageWidth->value(), pageHeight->value()),
                  unit,
                  QPageSize::name(psid),
                  QPageSize::ExactMatch);

      style.setPageSize(qps);
      style.pageOdd( )->setPageSize(*qps);
      style.pageEven()->setPageSize(*qps);

      if (isPreset) {
            QRectF rect = qps->rect(unit);
            updateWidthHeight(rect);
            updatePreview();
            }
      }
//---------------------------------------------------------
//   sizeChanged
//---------------------------------------------------------

void PageSettings::sizeChanged(int idx)
      {
      if (idx >= 0) // should never be -1, but just in case...
            setPageSize(QPageSize::PageSizeId(idx));
      }

//---------------------------------------------------------
//   widthHeightChanged - manually changing width/height = custom page size
//                        it can also change orientation
//---------------------------------------------------------

void PageSettings::widthHeightChanged(double w, double h)
{
      MStyle& style = preview->score()->style();
      bool    isP   = h >= w;      // Change orientation automatically
      bool    wasP  = portraitButton->isChecked();
      if (isP != wasP) {
            portraitButton ->blockSignals(true);
            landscapeButton->blockSignals(true);
            portraitButton ->setChecked( isP);
            landscapeButton->setChecked(!isP);
            portraitButton ->blockSignals(false);
            landscapeButton->blockSignals(false);
            QPageLayout::Orientation orient = isP ? QPageLayout::Portrait
                                                  : QPageLayout::Landscape;
            style.pageOdd() ->setOrientation(orient);
            style.pageEven()->setOrientation(orient);
      }

      ///!!!fuzzy match tolerance is +/-3pt. Now that there is a pageSize XML
      ///!!!tag, there is no need for fuzzy matches except reading older scores.
      ///!!!using exact matches here is cleaner for custom sizes.
      ///!!!QPageSize rounds inches and millimeters to 2 decimals, and points to
      ///!!!whole integers. Exact match is only that exact, which is good. For
      ///!!!example: using inches, entering 8.5 x 10.83 selects Quarto page size.
      int             idx  = unitsList->currentIndex();
      QSizeF          size = QSizeF(w, h); 
      QPageSize::Unit unit = QPageSize::Unit(idx);
      if (!isP)
            size.transpose();      // QPageSize should always be in portrait orientation
                                   // Is the new page size custom or preset?
      QPageSize::PageSizeId psid = QPageSize::id(size, unit, QPageSize::ExactMatch);
      if (int(psid) >= sizeCount)  // MuseScore crops the Qt list of page sizes
            psid = QPageSize::Custom;

      sizesList->blockSignals(true);
      sizesList->setCurrentIndex(int(psid));
      sizesList->blockSignals(false);
                                   // Create a new QPageSize instance
      QPageSize qps = (psid == QPageSize::Custom
                     ? QPageSize(size, unit, QPageSize::name(psid), QPageSize::ExactMatch)
                     : QPageSize(psid));

      style.setPageSize(&qps);     // update style variables
      style.pageOdd() ->setPageSize(qps);
      style.pageEven()->setPageSize(qps);

      updatePreview();
      }
void PageSettings::widthChanged(double val) {
      widthHeightChanged(val, pageHeight->value());
      }

void PageSettings::heightChanged(double val) {
      widthHeightChanged(pageWidth->value(), val);
      }

//// Margins /////////////////////////////////////////////
///!!!this code could be much more compact if the signals provided a "this",
///!!!the element, the spin box. There might be a way to do that in Qt with events...
//---------------------------------------------------------
//   marginMinMax - helper for all 8 margins' signals - handles out of range error
//---------------------------------------------------------
void PageSettings::marginMinMax(double val, double max, QPageLayout* layout, QDoubleSpinBox* spinner) {
      double minMax = val < 0 ? 0 : max;
      layout->setTopMargin(minMax);
      spinner->blockSignals(true);
      spinner->setValue(minMax);
      spinner->blockSignals(false);
      }
//// top and bottom first, they have no odd/even synchronzation
//---------------------------------------------------------
//   otmChanged - odd top margin
//---------------------------------------------------------

void PageSettings::otmChanged(double val)
      {
      QPageLayout* layout = preview->score()->style().pageOdd();
      if (!layout->setTopMargin(val)) // only error is out of range
            marginMinMax(val, layout->maximumMargins().top(), layout, oddPageTopMargin);
      updatePreview();
      }

//---------------------------------------------------------
//   obmChanged - odd bottom margin
//---------------------------------------------------------

void PageSettings::obmChanged(double val)
      {
      QPageLayout* layout = preview->score()->style().pageOdd();
      if (!layout->setBottomMargin(val)) // only error is out of range
            marginMinMax(val, layout->maximumMargins().bottom(), layout, oddPageBottomMargin);
      updatePreview();
}

//---------------------------------------------------------
//   etmChanged - even top margin
//---------------------------------------------------------

void PageSettings::etmChanged(double val)
      {
      QPageLayout* layout = preview->score()->style().pageEven();
      if (!layout->setTopMargin(val)) // only error is out of range
            marginMinMax(val, layout->maximumMargins().top(), layout, evenPageTopMargin);
      updatePreview();
      }

//---------------------------------------------------------
//   ebmChanged - even bottom margin
//---------------------------------------------------------

void PageSettings::ebmChanged(double val)
      {
      QPageLayout* layout = preview->score()->style().pageEven();
      if (!layout->setBottomMargin(val)) // only error is out of range
            marginMinMax(val, layout->maximumMargins().bottom(), layout, evenPageBottomMargin);
      updatePreview();
      }

//// Left and right margins synchronize (swapped) across odd/even pages
//---------------------------------------------------------
//   lrMargins - helper for the left/right margin valueChanged signals
//---------------------------------------------------------
void PageSettings::lrMargins(double val,
                             bool   isL,
                             bool   isOdd,
                             QPageLayout* one,
                             QPageLayout* other,
                             QDoubleSpinBox* spinOne)
      {
      bool b = isL ? one->setLeftMargin(val) : one->setRightMargin(val);
      if (!b) 
            marginMinMax(val,
                         isL ? one->maximumMargins().left() : one->maximumMargins().right(),
                         one,
                         spinOne);

      QDoubleSpinBox* spinOther;
      if (isL) {
            other->setRightMargin(val);
            spinOther = isOdd ? evenPageRightMargin : oddPageRightMargin;
            }
      else {
            other->setLeftMargin(val);
            spinOther = isOdd ? evenPageLeftMargin  : oddPageLeftMargin;
            }
      spinOther->blockSignals(true);
      spinOther->setValue(val);
      spinOther->blockSignals(false);

      updatePreview();
      }

//---------------------------------------------------------
//   olmChanged - odd left margin
//---------------------------------------------------------

void PageSettings::olmChanged(double val)
      {
      lrMargins(val,
                twosided->isChecked(),
                true,
                preview->score()->style().pageOdd(),
                preview->score()->style().pageEven(),
                oddPageLeftMargin);
      }

//---------------------------------------------------------
//   ormChanged - odd right margin
//---------------------------------------------------------

void PageSettings::ormChanged(double val)
      {
      lrMargins(val,
                !twosided->isChecked(),
                true,
                preview->score()->style().pageOdd(),
                preview->score()->style().pageEven(),
                oddPageRightMargin);
      }

//---------------------------------------------------------
//   elmChanged - even left margin
//---------------------------------------------------------

void PageSettings::elmChanged(double val)
      {
      lrMargins(val,
                twosided->isChecked(),
                false,
                preview->score()->style().pageEven(),
                preview->score()->style().pageOdd(),
                evenPageLeftMargin);
      }

//---------------------------------------------------------
//   ermChanged - even right margin
//---------------------------------------------------------

void PageSettings::ermChanged(double val)
      {
      lrMargins(val,
                !twosided->isChecked(),
                false,
                preview->score()->style().pageEven(),
                preview->score()->style().pageOdd(),
                evenPageRightMargin);
      }
//// end margins ////
//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void PageSettings::spatiumChanged(double val)
      { ///!!!QPageSize and all the other related classes round to 2 decimals.
        ///!!!that's perfect for everything but the spatium value.
      Score*            score = preview->score();
      QPageLayout::Unit unit  = score->style().pageOdd()->units();

      double newVal = val * units[int(unit)].factor() * DPI_F;
      double oldVal = score->spatium();
      switch (unit) {
            case QPageLayout::Millimeter: // rounding messes up the default value
                  if (val == 1.764) newVal = SPATIUM20;
                  break;
            case QPageLayout::Inch:
                  if (val == 0.069) newVal = SPATIUM20;
                  break;
            case QPageLayout::Pica:
                  if (val == 0.417) newVal = SPATIUM20;
                  break;
            case QPageLayout::Didot:
                  if (val == 4.692) newVal = SPATIUM20;
                  break;
            case QPageLayout::Cicero:
                  if (val == 0.391) newVal = SPATIUM20;
                  break;
            }
      score->setSpatium(newVal); 
      score->spatiumChanged(oldVal, newVal);
      updatePreview();
      }

//---------------------------------------------------------
//   pageOffsetChanged
//---------------------------------------------------------

void PageSettings::pageOffsetChanged(int val)
      {
      preview->score()->setPageNumberOffset(val-1);
      updatePreview(); ///!!!is this necessary??? the preview is tiny
      }

//---------------------------------------------------------
//   unitsChanged
//---------------------------------------------------------

void PageSettings::unitsChanged()
      {
      QPageLayout::Unit u = QPageLayout::Unit(unitsList->currentIndex());
      preview->score()->style().pageOdd() ->setUnits(u);
      preview->score()->style().pageEven()->setUnits(u);
      updateWidgets();
      }

//---------------------------------------------------------
//   updatePreview
//---------------------------------------------------------

void PageSettings::updatePreview()
      {
      preview->score()->doLayout();
      preview->layoutChanged();
      }
}

