//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//  $Id: pagesettings.cpp 5568 2012-04-22 10:08:43Z wschweer $
//
//  Copyright (C) 2002-2011 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "globals.h"
#include "pagesettings.h"
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
   : QDialog(parent)
      {
      setupUi(this);
      setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
      setModal(true);

      NScrollArea* sa = new NScrollArea;
      preview = new Navigator(sa, this);

      static_cast<QVBoxLayout*>(previewGroup->layout())->insertWidget(0, sa);

      _units = Units::MM; // should be made a global configuration item

      switch (_units) {
      case Units::MM :
            mmButton->setChecked(true);
            break;
      case Units::INCH :
            inchButton->setChecked(true);
            break;
      default:
            pxButton->setChecked(true);
            break;
      }

      connect(inchButton,           SIGNAL(clicked()),            SLOT(inchClicked()));
      connect(mmButton,             SIGNAL(clicked()),            SLOT(mmClicked()));
      connect(pxButton,             SIGNAL(clicked()),            SLOT(pxClicked()));
      connect(buttonApply,          SIGNAL(clicked()),            SLOT(apply()));
      connect(buttonApplyToAllParts,SIGNAL(clicked()),            SLOT(applyToAllParts()));
      connect(buttonOk,             SIGNAL(clicked()),            SLOT(ok()));
      connect(landscape,            SIGNAL(toggled(bool)),        SLOT(landscapeToggled(bool)));
      connect(twosided,             SIGNAL(toggled(bool)),        SLOT(twosidedToggled(bool)));
      connect(pageHeight,           SIGNAL(valueChanged(double)), SLOT(pageHeightChanged(double)));
      connect(pageWidth,            SIGNAL(valueChanged(double)), SLOT(pageWidthChanged(double)));
      connect(oddPageTopMargin,     SIGNAL(valueChanged(double)), SLOT(otmChanged(double)));
      connect(oddPageBottomMargin,  SIGNAL(valueChanged(double)), SLOT(obmChanged(double)));
      connect(oddPageLeftMargin,    SIGNAL(valueChanged(double)), SLOT(olmChanged(double)));
      connect(oddPageRightMargin,   SIGNAL(valueChanged(double)), SLOT(ormChanged(double)));
      connect(evenPageTopMargin,    SIGNAL(valueChanged(double)), SLOT(etmChanged(double)));
      connect(evenPageBottomMargin, SIGNAL(valueChanged(double)), SLOT(ebmChanged(double)));
      connect(evenPageRightMargin,  SIGNAL(valueChanged(double)), SLOT(ermChanged(double)));
      connect(evenPageLeftMargin,   SIGNAL(valueChanged(double)), SLOT(elmChanged(double)));
      connect(pageGroup,            SIGNAL(activated(int)),       SLOT(pageFormatSelected(int)));
      connect(spatiumEntry,         SIGNAL(valueChanged(double)), SLOT(spatiumChanged(double)));
      connect(pageOffsetEntry,      SIGNAL(valueChanged(int)),    SLOT(pageOffsetChanged(int)));
      }

//---------------------------------------------------------
//   PageSettings
//---------------------------------------------------------

PageSettings::~PageSettings()
      {
      }

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void PageSettings::setScore(Score* s)
      {
      cs  = s;
      Score* sl = s->clone();
      preview->setScore(sl);

      const PageFormat* pf = s->pageFormat();
      pageGroup->clear();
      int index = 0;
      const PaperSize* ps = pf->paperSize();
      for (int i = 0; true; ++i) {
            if (paperSizes[i].name == 0)
                  break;
            if (ps == &paperSizes[i])
                  index = i;
            pageGroup->addItem(QString(paperSizes[i].name));
            }

      pageGroup->setCurrentIndex(index);
      buttonApplyToAllParts->setEnabled(s->parentScore() != nullptr);
      updateValues();
      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   blockSignals
//---------------------------------------------------------

void PageSettings::blockSignals(bool block)
      {
      oddPageTopMargin->blockSignals(block);
      oddPageBottomMargin->blockSignals(block);
      oddPageLeftMargin->blockSignals(block);
      oddPageRightMargin->blockSignals(block);
      evenPageTopMargin->blockSignals(block);
      evenPageBottomMargin->blockSignals(block);
      evenPageLeftMargin->blockSignals(block);
      evenPageRightMargin->blockSignals(block);
      spatiumEntry->blockSignals(block);
      pageWidth->blockSignals(block);
      pageHeight->blockSignals(block);
      pageOffsetEntry->blockSignals(block);
      pageGroup->blockSignals(block);
// ???Radio button signals not blocked???
      }

//---------------------------------------------------------
//   updateValues
//---------------------------------------------------------

void PageSettings::updateValues()
      {
      blockSignals(true);

      double converter = convertBy();
      const QString suffix = unitSuffixes[(int)_units];

      double singleStepSize;
      double singleStepScale;
      switch (_units) {
      case Units::MM :
            singleStepSize  = 1.0;
            singleStepScale = 0.2;
            break;
      case Units::INCH :
            singleStepSize  = 0.05;
            singleStepScale = 0.005;
            break;
      default: // Units::PX
            singleStepSize  = 1; // no fractional points/pixels for now
            singleStepScale = 1;
            break;
      }

      pageWidth->setSuffix(suffix);
      pageWidth->setSingleStep(singleStepSize);
      pageHeight->setSuffix(suffix);
      pageHeight->setSingleStep(singleStepSize);
      oddPageTopMargin->setSuffix(suffix);
      oddPageTopMargin->setSingleStep(singleStepSize);
      oddPageBottomMargin->setSuffix(suffix);
      oddPageBottomMargin->setSingleStep(singleStepSize);
      oddPageLeftMargin->setSuffix(suffix);
      oddPageLeftMargin->setSingleStep(singleStepSize);
      oddPageRightMargin->setSuffix(suffix);
      oddPageRightMargin->setSingleStep(singleStepSize);
      evenPageTopMargin->setSuffix(suffix);
      evenPageTopMargin->setSingleStep(singleStepSize);
      evenPageBottomMargin->setSuffix(suffix);
      evenPageBottomMargin->setSingleStep(singleStepSize);
      evenPageLeftMargin->setSuffix(suffix);
      evenPageLeftMargin->setSingleStep(singleStepSize);
      evenPageRightMargin->setSuffix(suffix);
      evenPageRightMargin->setSingleStep(singleStepSize);

      spatiumEntry->setSuffix(suffix);
      spatiumEntry->setSingleStep(singleStepScale);

      const Score* sc = preview->score();
      const PageFormat* pf = sc->pageFormat();
      landscape->setChecked(pf->width() > pf->height());
      pageOffsetEntry->setValue(sc->pageNumberOffset() + 1);

      int index = 0;
      const PaperSize* ps = pf->paperSize();
      for (int i = 0; true; ++i) {
            if (ps == &paperSizes[i]) {
                  index = i;
                  break;
                  }
            }
      pageGroup->setCurrentIndex(index);

      spatiumEntry->setValue(sc->spatium() / converter);

      qreal widthValue = pf->size().width() / converter;
      pageWidth->setValue(widthValue);
      oddPageLeftMargin->setMaximum(widthValue);
      oddPageRightMargin->setMaximum(widthValue);
      evenPageLeftMargin->setMaximum(widthValue);
      evenPageRightMargin->setMaximum(widthValue);

      // ???Why no setMaximum for height???
      pageHeight->setValue(pf->size().height() / converter);

      oddPageTopMargin->setValue(    pf->oddTopMargin()     / converter);
      oddPageBottomMargin->setValue( pf->oddBottomMargin()  / converter);
      oddPageLeftMargin->setValue(   pf->oddLeftMargin()    / converter);
      oddPageRightMargin->setValue(  pf->oddRightMargin()   / converter);

      evenPageTopMargin->setValue(   pf->evenTopMargin()    / converter);
      evenPageBottomMargin->setValue(pf->evenBottomMargin() / converter);
      evenPageLeftMargin->setValue(  pf->evenLeftMargin()   / converter);
      evenPageRightMargin->setValue( pf->evenRightMargin()  / converter);

      bool twoFace = pf->twosided();
      twosided->setChecked(twoFace);
      evenPageTopMargin->setEnabled(twoFace);
      evenPageBottomMargin->setEnabled(twoFace);
      evenPageLeftMargin->setEnabled(twoFace);
      evenPageRightMargin->setEnabled(twoFace);

      qreal oddLeft  = oddPageLeftMargin->value();
      qreal oddRight = oddPageRightMargin->value();
      if (twoFace) {
            evenPageRightMargin->setValue(oddLeft);
            evenPageLeftMargin->setValue(oddRight);
            }
      else {
            evenPageRightMargin->setValue(oddRight);
            evenPageLeftMargin->setValue(oddLeft);
            }

      blockSignals(false);
      }

//---------------------------------------------------------
//   inchClicked
//---------------------------------------------------------

void PageSettings::inchClicked()
      {
      _units = Units::INCH;
      updateValues();
      }

//---------------------------------------------------------
//   mmClicked
//---------------------------------------------------------

void PageSettings::mmClicked()
      {
      _units = Units::MM;
      updateValues();
      }

//---------------------------------------------------------
//   pxClicked
//---------------------------------------------------------

void PageSettings::pxClicked()
      {
      _units = Units::PX;
      updateValues();
      }

//---------------------------------------------------------
//   landscapeToggled
//---------------------------------------------------------

void PageSettings::landscapeToggled(bool flag)
      {
      PageFormat* pf = preview->score()->pageFormat();
      if (flag ^ (pf->width() > pf->height()))
            pf->setSize(QSizeF(pf->height(), pf->width()));
      pf->setPrintableWidth(pf->width() - (pf->oddLeftMargin() + pf->oddRightMargin()));
      updateValues();
      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   twosidedToggled
//---------------------------------------------------------

void PageSettings::twosidedToggled(bool flag)
      {
      preview->score()->pageFormat()->setTwosided(flag);
      updateValues();
      updatePreview(PREVIEW_HEIGHT);
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
      s->undoChangePageFormat(preview->score()->pageFormat(),
                              preview->score()->spatium(),
                              pageOffsetEntry->value()-1);
      s->endCmd();
      }

//---------------------------------------------------------
//   applyToAllParts
//---------------------------------------------------------

void PageSettings::applyToAllParts()
      {
      for (Excerpt* e : cs->rootScore()->excerpts())
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
      cs->setLayoutAll(true);     // HACK
      QDialog::done(val);
      }

//---------------------------------------------------------
//   pageFormatSelected
//---------------------------------------------------------

void PageSettings::pageFormatSelected(int size)
      {
      if (size != Page::SIZE_CUSTOM) { // Custom maintains the size of the page
            PageFormat* pf = preview->score()->pageFormat();
            pf->setSize(&paperSizes[size]);
            pf->setPrintableWidth(pf->width() - (pf->oddLeftMargin() + pf->oddRightMargin()));
            updateValues();
            updatePreview(PREVIEW_WIDTH);
            }
      }

//---------------------------------------------------------
//   otmChanged
//---------------------------------------------------------

void PageSettings::otmChanged(double val)
      {
      preview->score()->pageFormat()->setOddTopMargin(convertToPx(val));
      updatePreview(PREVIEW_HEIGHT);
      }

//---------------------------------------------------------
//   olmChanged
//---------------------------------------------------------

void PageSettings::olmChanged(double val)
      {
      if(twosided->isChecked()) {
            evenPageRightMargin->blockSignals(true);
            evenPageRightMargin->setValue(val);
            evenPageRightMargin->blockSignals(false);
            }
      else{
            evenPageLeftMargin->blockSignals(true);
            evenPageLeftMargin->setValue(val);
            evenPageLeftMargin->blockSignals(false);
            }
      PageFormat* pf = preview->score()->pageFormat();
      double   pxVal = convertToPx(val);
      pf->setPrintableWidth(pf->width() - pf->oddRightMargin() - pxVal);
      pf->setOddLeftMargin(pxVal);

      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   ormChanged
//---------------------------------------------------------

void PageSettings::ormChanged(double val)
      {
      PageFormat* pf = preview->score()->pageFormat();
      double   pxVal = convertToPx(val);

      if (twosided->isChecked()) {
            evenPageLeftMargin->blockSignals(true);
            evenPageLeftMargin->setValue(val);
            pf->setEvenLeftMargin(pxVal);
            evenPageLeftMargin->blockSignals(false);
            }
      else {
            evenPageRightMargin->blockSignals(true);
            evenPageRightMargin->setValue(val);
            evenPageRightMargin->blockSignals(false);
            }
      pf->setPrintableWidth(pf->width() - pf->oddLeftMargin() - pxVal);
      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   obmChanged
//---------------------------------------------------------

void PageSettings::obmChanged(double val)
      {
      preview->score()->pageFormat()->setOddBottomMargin(convertToPx(val));
      updatePreview(PREVIEW_HEIGHT);
      }

//---------------------------------------------------------
//   etmChanged
//---------------------------------------------------------

void PageSettings::etmChanged(double val)
      {
      preview->score()->pageFormat()->setEvenTopMargin(convertToPx(val));
      updatePreview(PREVIEW_HEIGHT);
      }

//---------------------------------------------------------
//   elmChanged
//---------------------------------------------------------

void PageSettings::elmChanged(double val)
      {
      if(twosided->isChecked()) {
            oddPageRightMargin->blockSignals(true);
            oddPageRightMargin->setValue(val);
            oddPageRightMargin->blockSignals(false);
            }
      PageFormat* pf = preview->score()->pageFormat();
      double   pxVal = convertToPx(val);
      pf->setPrintableWidth(pf->width() - pf->evenRightMargin() - pxVal);
      pf->setEvenLeftMargin(pxVal);

      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   ermChanged
//---------------------------------------------------------

void PageSettings::ermChanged(double val)
      {
      if (twosided->isChecked()) {
            oddPageLeftMargin->blockSignals(true);
            oddPageLeftMargin->setValue(val);
            oddPageLeftMargin->blockSignals(false);
            }
      PageFormat* pf = preview->score()->pageFormat();
      double   pxVal = convertToPx(val);
      pf->setPrintableWidth(pf->width() - pf->evenLeftMargin() - pxVal);
      pf->setOddLeftMargin(pxVal);
      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   ebmChanged
//---------------------------------------------------------

void PageSettings::ebmChanged(double val)
      {
      preview->score()->pageFormat()->setEvenBottomMargin(convertToPx(val));
      updatePreview(PREVIEW_HEIGHT);
      }

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void PageSettings::spatiumChanged(double val)
      {
      double newVal = convertToPx(val);
      double oldVal = preview->score()->spatium();
      preview->score()->setSpatium(newVal);
      preview->score()->spatiumChanged(oldVal, newVal);
      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   pageOffsetChanged
//---------------------------------------------------------

void PageSettings::pageOffsetChanged(int val)
      {
      preview->score()->setPageNumberOffset(val-1);
      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   pageHeightChanged
//---------------------------------------------------------

void PageSettings::pageHeightChanged(double val)
      {
      pageGroup->setCurrentIndex(Page::SIZE_CUSTOM);
      preview->score()->pageFormat()->setSize(QSizeF(convertToPx(pageWidth->value()),
                                                     convertToPx(val)));
      updatePreview(PREVIEW_HEIGHT);
      }

//---------------------------------------------------------
//   pageWidthChanged
//---------------------------------------------------------

void PageSettings::pageWidthChanged(double val)
      {
      pageGroup->setCurrentIndex(Page::SIZE_CUSTOM);
      preview->score()->pageFormat()->setSize(QSizeF(convertToPx(val),
                                                     convertToPx(pageHeight->value())));
      oddPageLeftMargin->setMaximum(val);
      oddPageRightMargin->setMaximum(val);
      evenPageLeftMargin->setMaximum(val);
      evenPageRightMargin->setMaximum(val);

      updatePreview(PREVIEW_WIDTH);
      }

//---------------------------------------------------------
//   updatePreview
//---------------------------------------------------------

void PageSettings::updatePreview(int val)
      {
//      updateValues();
      switch(val) {
            case PREVIEW_WIDTH:
                  preview->score()->doLayout();
                  break;
            case PREVIEW_HEIGHT:
                  preview->score()->doLayoutPages();
                  break;
            }
      preview->layoutChanged();
      }

//
// Converts pixels/points to user units (the units selected in this dialog)
//
double PageSettings::convertToUser(double val)
{
    return val / convertBy();
}

//
// Converts user units to pixels/points
//
double PageSettings::convertToPx(double val)
{
    return val * convertBy();
}

//
// Returns the conversion factor
//
double PageSettings::convertBy()
{
    switch (_units) {
    case Units::MM :
        return DPMM;
        break;
    case Units::INCH :
        return DPI;
        break;
    default: // Units::PX
        return 1; // No conversion
        break;
    }
}

} //namespace Ms
