//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2014 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

/**
 File handling: loading and saving.
 */

#include "config.h"
#include "globals.h"
#include "musescore.h"
#include "scoreview.h"
#include "exportmidi.h"
#include "libmscore/xml.h"
#include "libmscore/element.h"
#include "libmscore/note.h"
#include "libmscore/rest.h"
#include "libmscore/sig.h"
#include "libmscore/clef.h"
#include "libmscore/key.h"
#include "instrdialog.h"
#include "libmscore/score.h"
#include "libmscore/page.h"
#include "libmscore/dynamic.h"
#include "file.h"
#include "libmscore/style.h"
#include "libmscore/tempo.h"
#include "libmscore/select.h"
#include "preferences.h"
#include "playpanel.h"
#include "libmscore/staff.h"
#include "libmscore/part.h"
#include "libmscore/utils.h"
#include "libmscore/barline.h"
#include "palette.h"
#include "symboldialog.h"
#include "libmscore/slur.h"
#include "libmscore/hairpin.h"
#include "libmscore/ottava.h"
#include "libmscore/textline.h"
#include "libmscore/pedal.h"
#include "libmscore/trill.h"
#include "libmscore/volta.h"
#include "newwizard.h"
#include "libmscore/timesig.h"
#include "libmscore/box.h"
#include "libmscore/excerpt.h"
#include "libmscore/system.h"
#include "libmscore/tuplet.h"
#include "libmscore/keysig.h"
#include "magbox.h"
#include "libmscore/measure.h"
#include "libmscore/undo.h"
#include "libmscore/repeatlist.h"
#include "scoretab.h"
#include "libmscore/beam.h"
#include "libmscore/stafftype.h"
#include "seq.h"
#include "libmscore/revisions.h"
#include "libmscore/lyrics.h"
#include "libmscore/segment.h"
#include "libmscore/tempotext.h"
#include "libmscore/sym.h"
#include "libmscore/image.h"
#include "synthesizer/msynthesizer.h"
#include "svggenerator.h"
#include "scorePreview.h"

#ifdef OMR
#include "omr/omr.h"
#include "omr/omrpage.h"
#include "omr/importpdf.h"
#endif

#include "diff/diff_match_patch.h"
#include "libmscore/chordlist.h"
#include "libmscore/mscore.h"
#include "thirdparty/qzip/qzipreader_p.h"

//////////////////////////////////
// SMAWS includes and defines
#include "libmscore/tie.h"
#include "libmscore/stem.h"
#include "libmscore/hook.h"
#include "libmscore/tempo.h"
#include "libmscore/chord.h"
#include "libmscore/iname.h"
#include "libmscore/harmony.h"
#include "libmscore/notedot.h"
#include "libmscore/accidental.h"
#include "libmscore/instrchange.h"

// Not defined elsewhere in MuseScore...
#define tagWorkNo "workNumber"
#define tagMoveNo "movementNumber"

// SMAWS export/file types - stored in a score's movementNumber metaTag
#define SMAWS_RULERS "Rulers"
#define SMAWS_TREE   "MixTree"
#define SMAWS_SCORE  "Score"
#define SMAWS_GRID   "Grid"
#define SMAWS_FRETS  "Frets"
#define SMAWS_PART   "Part"
#define SMAWS_LYRICS "Lyrics"
#define SMAWS_VIDEO  "Video"
#define SMAWS_       '_'    // separates workNo from moveNo in filename

// For QFileDialog. See MuseScore::exportFile() below
#define EXT_SVG  ".svg"
#define EXT_VTT  ".vtt"
#define EXT_HTML ".html"

#define FILE_LYRICS     "SMAWS_Lyrics.svg.txt"        // lyrics svg boilerplate - complete file
#define FILE_RULER_HDR  "SMAWS_RulerHdr.svg.txt"      // ruler svg boilerplate header/title
#define FILE_RULER_G    "SMAWS_RulerHdrG.svg.txt"     // initial elements for each ruler <g>
#define FILE_PLAY_BUTTS "SMAWS_PlayButts.svg.txt"     // sheet music playback buttons
#define FILE_FRET_DEFS  "SMAWS_FretsDefs.svg.txt"     // Fretboard <defs>
#define FILE_FRET_BUTTS "SMAWS_FretsButts.svg.txt"    // Fretboard buttons
#define FILE_FRETS_12_6 "SMAWS_Frets12-6.svg.txt"     // 12-fret, 6-string fretboard template
#define FILE_FRETS_12_4 "SMAWS_Frets12-4.svg.txt"     // 12-fret, 4-string fretboard template
#define FILE_FRETS_14_6 "SMAWS_Frets14-6.svg.txt"     // 14-fret, 6-string fretboard template
#define FILE_FRETS_14_4 "SMAWS_Frets14-4.svg.txt"     // 14-fret, 4-string fretboard template
#define FILE_GRID_DEFS  "SMAWS_GridDefs.svg.txt"      // Grid <defs>
#define FILE_GRID_PLAY  "SMAWS_GridPlayButts.svg.txt" // Grid playback buttons
#define FILE_GRID_TEMPO "SMAWS_GridTempo.svg.txt"     // Grid page buttons
#define FILE_GRID_BOTH  "SMAWS_GridPageButts.svg.txt" // Grid playback + page buttons
#define FILE_GRID_INST  "SMAWS_GridInst.svg.txt"      // Grid controls by row/channel

#define FILTER_SMAWS_AUTO_OPEN   "Auto-SMAWS: Open Files"
#define FILTER_SMAWS_AUTO_ALL    "Auto-SMAWS:  All Files"
#define FILTER_SMAWS             "SMAWS SVG+VTT"
#define FILTER_SMAWS_MULTI       "SMAWS Multi-Staff"
#define FILTER_SMAWS_RULERS      "SMAWS Rulers"
#define FILTER_SMAWS_TABLES      "SMAWS HTML Tables"
#define FILTER_SMAWS_GRID        "SMAWS SVG Grids"
#define FILTER_SMAWS_GRID_RULERS "SMAWS SVG Grids w/built-in Rulers"
#define FILTER_SMAWS_FRETS       "SMAWS Fretboard"
#define FILTER_SMAWS_MIX_TREE    "SMAWS MixTree"
#define FILTER_SMAWS_LYRICS      "SMAWS Lyrics"

#define SMAWS_DESC_STUB "&#x00A9;%1 - generated by MuseScore %2 + SMAWS&#x2122; %3"

// For Cue ID formatting
#define CUE_ID_FIELD_WIDTH 7
#define VTT_CUE_3_ARGS     "%1\n%2 --> %3\n"

// For Frozen Pane formatting
#define WIDTH_CLEF     16
#define WIDTH_KEY_SIG   5
#define WIDTH_TIME_SIG 10
#define X_OFF_TIME_SIG  3

// SMAWS end
//////////////////////////////////

namespace Ms {

extern void importSoundfont(QString name);
extern bool savePositions(Score*, const QString& name, bool segments);
extern MasterSynthesizer* synti;

//---------------------------------------------------------
//   paintElement(s)
//---------------------------------------------------------

static void paintElement(QPainter& p, const Element* e)
      {
      QPointF pos(e->pagePos());
      p.translate(pos);
      e->draw(&p);
      p.translate(-pos);
      }

static void paintElements(QPainter& p, const QList<Element*>& el)
      {
      for (Element* e : el) {
            if (!e->visible())
                  continue;
            paintElement(p, e);
            }
      }

//---------------------------------------------------------
//   createDefaultFileName
//---------------------------------------------------------

static QString createDefaultFileName(QString fn)
      {
      //
      // special characters in filenames are a constant source
      // of trouble, this replaces some of them common in german:
      //
      fn = fn.simplified();
      fn = fn.replace(QChar(' '),  "_");
      fn = fn.replace(QChar('\n'), "_");
      fn = fn.replace(QChar(0xe4), "ae"); // &auml;
      fn = fn.replace(QChar(0xf6), "oe"); // &ouml;
      fn = fn.replace(QChar(0xfc), "ue"); // &uuml;
      fn = fn.replace(QChar(0xdf), "ss"); // &szlig;
      fn = fn.replace(QChar(0xc4), "Ae"); // &Auml;
      fn = fn.replace(QChar(0xd6), "Oe"); // &Ouml;
      fn = fn.replace(QChar(0xdc), "Ue"); // &Uuml;
      fn = fn.replace(QChar(0x266d),"b"); // musical flat sign, happen in instrument names, so can happen in part (file) names
      fn = fn.replace(QChar(0x266f),"#"); // musical sharp sign, can happen in titles, so can happen in score (file) names
      fn = fn.replace( QRegExp( "[" + QRegExp::escape( "\\/:*?\"<>|" ) + "]" ), "_" ); //FAT/NTFS special chars
      return fn;
      }

//---------------------------------------------------------
//   readScoreError
//    if "ask" is true, ask to ignore; returns true if
//    ignore is pressed by user
//    returns true if -f is used in converter mode
//---------------------------------------------------------

static bool readScoreError(const QString& name, Score::FileError error, bool ask)
      {
      QString msg = QObject::tr("Cannot read file %1:\n").arg(name);
      QString detailedMsg;
      bool canIgnore = false;
      switch(error) {
            case Score::FileError::FILE_NO_ERROR:
                  return false;
            case Score::FileError::FILE_BAD_FORMAT:
                  msg +=  QObject::tr("bad format");
                  detailedMsg = MScore::lastError;
                  break;
            case Score::FileError::FILE_UNKNOWN_TYPE:
                  msg += QObject::tr("unknown type");
                  break;
            case Score::FileError::FILE_NO_ROOTFILE:
                  break;
            case Score::FileError::FILE_TOO_OLD:
                  msg += QObject::tr("It was last saved with a version older than 2.0.0.\n"
                                     "You can convert this score by opening and then\n"
                                     "saving with MuseScore version 2.x.\n"
                                     "Visit the %1MuseScore download page%2 to obtain such a 2.x version.")
                              .arg("<a href=\"http://musescore.org/download#older-versions\">")
                              .arg("</a>");
                  canIgnore = true;
                  break;
            case Score::FileError::FILE_TOO_NEW:
                  msg += QObject::tr("This score was saved using a newer version of MuseScore.\n"
                                     "Visit the %1MuseScore website%2 to obtain the latest version.")
                              .arg("<a href=\"http://musescore.org\">")
                              .arg("</a>");
                  canIgnore = true;
                  break;
            case Score::FileError::FILE_NOT_FOUND:
                  msg = QObject::tr("File not found %1").arg(name);
                  break;
            case Score::FileError::FILE_CORRUPTED:
                  msg = QObject::tr("File corrupted %1").arg(name);
                  detailedMsg = MScore::lastError;
                  canIgnore = true;
                  break;
            case Score::FileError::FILE_ERROR:
            case Score::FileError::FILE_OPEN_ERROR:
            default:
                  msg += MScore::lastError;
                  break;
            }
      if (converterMode && canIgnore && ignoreWarnings) {
            fprintf(stderr, "%s\n\nWarning ignored, forcing score to load\n", qPrintable(msg));
            return true;
            }
       if (converterMode || pluginMode) {
            fprintf(stderr, "%s\n", qPrintable(msg));
            return false;
            }
      QMessageBox msgBox;
      msgBox.setWindowTitle(QObject::tr("MuseScore: Load Error"));
      msgBox.setText(msg.replace("\n", "<br/>"));
      msgBox.setDetailedText(detailedMsg);
      msgBox.setTextFormat(Qt::RichText);
      if (canIgnore && ask)  {
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(
               QMessageBox::Cancel | QMessageBox::Ignore
               );
            return msgBox.exec() == QMessageBox::Ignore;
            }
      else {
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setStandardButtons(
               QMessageBox::Ok
               );
            msgBox.exec();
            }
      return false;
      }

//---------------------------------------------------------
//   checkDirty
//    if dirty, save score
//    return true on cancel
//---------------------------------------------------------

bool MuseScore::checkDirty(Score* s)
      {
      if (s->dirty() || s->created()) {
            QMessageBox::StandardButton n = QMessageBox::warning(this, tr("MuseScore"),
               tr("Save changes to the score \"%1\"\n"
               "before closing?").arg(s->fileInfo()->completeBaseName()),
               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
               QMessageBox::Save);
            if (n == QMessageBox::Save) {
                  if (s->masterScore()->isSavable()) {
                        if (!saveFile(s))
                              return true;
                        }
                  else {
                        if (!saveAs(s, false))
                              return true;
                        }

                  }
            else if (n == QMessageBox::Cancel)
                  return true;
            }
      return false;
      }

//---------------------------------------------------------
//   loadFile
//---------------------------------------------------------

/**
 Create a modal file open dialog.
 If a file is selected, load it.
 Handles the GUI's file-open action.
 */

void MuseScore::loadFiles()
      {
      QStringList files = getOpenScoreNames(
#ifdef OMR
         tr("All Supported Files") + " (*.mscz *.mscx *.xml *.mxl *.mid *.midi *.kar *.md *.mgu *.MGU *.sgu *.SGU *.cap *.capx *.pdf *.ove *.scw *.bww *.GTP *.GP3 *.GP4 *.GP5 *.GPX);;" +
#else
         tr("All Supported Files") + " (*.mscz *.mscx *.xml *.mxl *.mid *.midi *.kar *.md *.mgu *.MGU *.sgu *.SGU *.cap *.capx *.ove *.scw *.bww *.GTP *.GP3 *.GP4 *.GP5 *.GPX);;" +
#endif
         tr("MuseScore Files") + " (*.mscz *.mscx);;" +
         tr("MusicXML Files") + " (*.xml *.mxl);;" +
         tr("MIDI Files") + " (*.mid *.midi *.kar);;" +
         tr("Muse Data Files") + " (*.md);;" +
         tr("Capella Files") + " (*.cap *.capx);;" +
         tr("BB Files <experimental>") + " (*.mgu *.MGU *.sgu *.SGU);;" +
#ifdef OMR
         tr("PDF Files <experimental OMR>") + " (*.pdf);;" +
#endif
         tr("Overture / Score Writer Files <experimental>") + " (*.ove *.scw);;" +
         tr("Bagpipe Music Writer Files <experimental>") + " (*.bww);;" +
         tr("Guitar Pro") + " (*.GTP *.GP3 *.GP4 *.GP5 *.GPX)",
         tr("MuseScore: Load Score")
         );
      for (const QString& s : files)
            openScore(s);
      }

//---------------------------------------------------------
//   openScore
//---------------------------------------------------------

Score* MuseScore::openScore(const QString& fn)
      {
      //
      // make sure we load a file only once
      //
      QFileInfo fi(fn);
      QString path = fi.canonicalFilePath();
      for (Score* s : scoreList) {
            if (s->masterScore()->fileInfo()->canonicalFilePath() == path)
                  return 0;
            }

      MasterScore* score = readScore(fn);
      if (score) {
            setCurrentScoreView(appendScore(score));
            writeSessionFile(false);
            }
      return score;
      }

//---------------------------------------------------------
//   readScore
//---------------------------------------------------------

MasterScore* MuseScore::readScore(const QString& name)
      {
      if (name.isEmpty())
            return 0;

      MasterScore* score = new MasterScore(MScore::baseStyle());  // start with built-in style
      setMidiReopenInProgress(name);
      Score::FileError rv = Ms::readScore(score, name, false);
      if (rv == Score::FileError::FILE_TOO_OLD || rv == Score::FileError::FILE_TOO_NEW || rv == Score::FileError::FILE_CORRUPTED) {
            if (readScoreError(name, rv, true)) {
                  if (rv != Score::FileError::FILE_CORRUPTED) {
                        // dont read file again if corrupted
                        // the check routine may try to fix it
                        delete score;
                        score = new MasterScore(MScore::baseStyle());
                        rv = Ms::readScore(score, name, true);
                        }
                  else
                        rv = Score::FileError::FILE_NO_ERROR;
                  }
            else {
                  delete score;
                  return 0;
                  }
            }
      if (rv != Score::FileError::FILE_NO_ERROR) {
            // in case of user abort while reading, the error has already been reported
            // else report it now
            if (rv != Score::FileError::FILE_USER_ABORT && rv != Score::FileError::FILE_IGNORE_ERROR)
                  readScoreError(name, rv, false);
            delete score;
            score = 0;
            return 0;
            }
      allowShowMidiPanel(name);
      if (score)
            addRecentScore(score);
      return score;
      }

//---------------------------------------------------------
//   saveFile
///   Save the current score.
///   Handles the GUI's file-save action.
//
//    return true on success
//---------------------------------------------------------

bool MuseScore::saveFile()
      {
      return saveFile(cs->masterScore());
      }

//---------------------------------------------------------
//   saveFile
///   Save the score.
//
//    return true on success
//---------------------------------------------------------

bool MuseScore::saveFile(Score* score)
      {
      if (score == 0)
            return false;
      if (score->created()) {
            QString fn = score->masterScore()->fileInfo()->fileName();
            Text* t = score->getText(TextStyleType::TITLE);
            if (t)
                  fn = t->plainText(true);
            QString name = createDefaultFileName(fn);
            QString f1 = tr("MuseScore File") + " (*.mscz)";
            QString f2 = tr("Uncompressed MuseScore File") + " (*.mscx)";

            QSettings settings;
            if (mscore->lastSaveDirectory.isEmpty())
                  mscore->lastSaveDirectory = settings.value("lastSaveDirectory", preferences.myScoresPath).toString();
            QString saveDirectory = mscore->lastSaveDirectory;

            if (saveDirectory.isEmpty())
                  saveDirectory = preferences.myScoresPath;

            QString fname = QString("%1/%2").arg(saveDirectory).arg(name);
            QString filter = f1 + ";;" + f2;
            if (QFileInfo(fname).suffix().isEmpty())
                  fname += ".mscz";

            fn = mscore->getSaveScoreName(tr("MuseScore: Save Score"), fname, filter);
            if (fn.isEmpty())
                  return false;
            score->masterScore()->fileInfo()->setFile(fn);

            mscore->lastSaveDirectory = score->masterScore()->fileInfo()->absolutePath();

            if (!score->masterScore()->saveFile()) {
                  QMessageBox::critical(mscore, tr("MuseScore: Save File"), MScore::lastError);
                  return false;
                  }
            addRecentScore(score);
            writeSessionFile(false);
            }
      else if (!score->masterScore()->saveFile()) {
            QMessageBox::critical(mscore, tr("MuseScore: Save File"), MScore::lastError);
            return false;
            }
      score->setCreated(false);
      setWindowTitle("MuseScore: " + score->fileInfo()->completeBaseName());
      int idx = scoreList.indexOf(score->masterScore());
      tab1->setTabText(idx, score->fileInfo()->completeBaseName());
      if (tab2)
            tab2->setTabText(idx, score->fileInfo()->completeBaseName());
      QString tmp = score->tmpName();
      if (!tmp.isEmpty()) {
            QFile f(tmp);
            if (!f.remove())
                  qDebug("cannot remove temporary file <%s>", qPrintable(f.fileName()));
            score->setTmpName("");
            }
      writeSessionFile(false);
      return true;
      }

//---------------------------------------------------------
//   createDefaultName
//---------------------------------------------------------

QString MuseScore::createDefaultName() const
      {
      QString name(tr("Untitled"));
      int n;
      for (n = 1; ; ++n) {
            bool nameExists = false;
            QString tmpName;
            if (n == 1)
                  tmpName = name;
            else
                  tmpName = QString("%1-%2").arg(name).arg(n);
            foreach(Score* s, scoreList) {
                  if (s->fileInfo()->completeBaseName() == tmpName) {
                        nameExists = true;
                        break;
                        }
                  }
            if (!nameExists) {
                  name = tmpName;
                  break;
                  }
            }
      return name;
      }


void MuseScore::updateNewWizard()
      {
      if (newWizard != 0)
            newWizard = new NewWizard(this);
      }

//---------------------------------------------------------
//   newFile
//    create new score
//---------------------------------------------------------

void MuseScore::newFile()
      {
      if (newWizard == 0)
            newWizard = new NewWizard(this);
      newWizard->restart();
      if (newWizard->exec() != QDialog::Accepted)
            return;
      int measures            = newWizard->measures();
      Fraction timesig        = newWizard->timesig();
      TimeSigType timesigType = newWizard->timesigType();
      KeySigEvent ks          = newWizard->keysig();
      VBox* nvb               = nullptr;

      int pickupTimesigZ;
      int pickupTimesigN;
      bool pickupMeasure = newWizard->pickupMeasure(&pickupTimesigZ, &pickupTimesigN);
      if (pickupMeasure)
            measures += 1;

      MasterScore* score;
      QString tp = newWizard->templatePath();

      QList<Excerpt*> excerpts;
      if (!newWizard->emptyScore()) {
            MasterScore* tscore = new MasterScore(MScore::defaultStyle());
            Score::FileError rv = Ms::readScore(tscore, tp, false);
            if (rv != Score::FileError::FILE_NO_ERROR) {
                  readScoreError(newWizard->templatePath(), rv, false);
                  delete tscore;
                  return;
                  }
            score = new MasterScore(tscore->style());
            // create instruments from template
            for (Part* tpart : tscore->parts()) {
                  Part* part = new Part(score);
                  part->setInstrument(tpart->instrument());
                  part->setPartName(tpart->partName());

                  for (Staff* tstaff : *tpart->staves()) {
                        Staff* staff = new Staff(score);
                        staff->setPart(part);
                        staff->init(tstaff);
                        if (tstaff->linkedStaves() && !part->staves()->isEmpty()) {
                              Staff* linkedStaff = part->staves()->back();
                              staff->linkTo(linkedStaff);
                              }
                        part->insertStaff(staff, -1);
                        score->staves().append(staff);
                        }
                  score->appendPart(part);
                  }
            for (Excerpt* ex : tscore->excerpts()) {
                  Excerpt* x = new Excerpt(score);
                  x->setTitle(ex->title());
                  for (Part* p : ex->parts()) {
                        int pidx = tscore->parts().indexOf(p);
                        if (pidx == -1)
                              qDebug("newFile: part not found");
                        else
                              x->parts().append(score->parts()[pidx]);
                        }
                  excerpts.append(x);
                  }
            MeasureBase* mb = tscore->first();
            if (mb && mb->type() == Element::Type::VBOX) {
                  VBox* tvb = static_cast<VBox*>(mb);
                  nvb = new VBox(score);
                  nvb->setBoxHeight(tvb->boxHeight());
                  nvb->setBoxWidth(tvb->boxWidth());
                  nvb->setTopGap(tvb->topGap());
                  nvb->setBottomGap(tvb->bottomGap());
                  nvb->setTopMargin(tvb->topMargin());
                  nvb->setBottomMargin(tvb->bottomMargin());
                  nvb->setLeftMargin(tvb->leftMargin());
                  nvb->setRightMargin(tvb->rightMargin());
                  }
            delete tscore;
            }
      else {
            score = new MasterScore(MScore::defaultStyle());
            newWizard->createInstruments(score);
            }
      score->setCreated(true);
      score->masterScore()->fileInfo()->setFile(createDefaultName());

      if (!score->style()->chordList()->loaded()) {
            if (score->style()->value(StyleIdx::chordsXmlFile).toBool())
                  score->style()->chordList()->read("chords.xml");
            score->style()->chordList()->read(score->style()->value(StyleIdx::chordDescriptionFile).toString());
            }
      if (!newWizard->title().isEmpty())
            score->masterScore()->fileInfo()->setFile(newWizard->title());

      score->sigmap()->add(0, timesig);

      int firstMeasureTicks = pickupMeasure ? Fraction(pickupTimesigZ, pickupTimesigN).ticks() : timesig.ticks();

      for (int i = 0; i < measures; ++i) {
            int tick = firstMeasureTicks + timesig.ticks() * (i - 1);
            if (i == 0)
                  tick = 0;
            QList<Rest*> puRests;
            for (Score* _score : score->scoreList()) {
                  Rest* rest = 0;
                  Measure* measure = new Measure(_score);
                  measure->setTimesig(timesig);
                  measure->setLen(timesig);
                  measure->setTick(tick);

                  if (pickupMeasure && tick == 0) {
                        measure->setIrregular(true);        // dont count pickup measure
                        measure->setLen(Fraction(pickupTimesigZ, pickupTimesigN));
                        }
                  _score->measures()->add(measure);

                  for (Staff* staff : _score->staves()) {
                        int staffIdx = staff->idx();
                        if (tick == 0) {
                              TimeSig* ts = new TimeSig(_score);
                              ts->setTrack(staffIdx * VOICES);
                              ts->setSig(timesig, timesigType);
                              Measure* m = _score->firstMeasure();
                              Segment* s = m->getSegment(ts, 0);
                              s->add(ts);
                              Part* part = staff->part();
                              if (!part->instrument()->useDrumset()) {
                                    //
                                    // transpose key
                                    //
                                    KeySigEvent nKey = ks;
                                    if (!nKey.custom() && !nKey.isAtonal() && part->instrument()->transpose().chromatic && !score->styleB(StyleIdx::concertPitch)) {
                                          int diff = -part->instrument()->transpose().chromatic;
                                          nKey.setKey(transposeKey(nKey.key(), diff));
                                          }
                                    // do not create empty keysig unless custom or atonal
                                    if (nKey.custom() || nKey.isAtonal() || nKey.key() != Key::C) {
                                          staff->setKey(0, nKey);
                                          KeySig* keysig = new KeySig(score);
                                          keysig->setTrack(staffIdx * VOICES);
                                          keysig->setKeySigEvent(nKey);
                                          Segment* s = measure->getSegment(keysig, 0);
                                          s->add(keysig);
                                          }
                                    }
                              }

                        // determined if this staff is linked to previous so we can reuse rests
                        bool linkedToPrevious = staffIdx && staff->isLinked(_score->staff(staffIdx - 1));
                        if (measure->timesig() != measure->len()) {
                              if (!linkedToPrevious)
                                    puRests.clear();
                              std::vector<TDuration> dList = toDurationList(measure->len(), false);
                              if (!dList.empty()) {
                                    int ltick = tick;
                                    int k = 0;
                                    foreach (TDuration d, dList) {
                                          if (k < puRests.count())
                                                rest = static_cast<Rest*>(puRests[k]->linkedClone());
                                          else {
                                                rest = new Rest(score, d);
                                                puRests.append(rest);
                                                }
                                          rest->setScore(_score);
                                          rest->setDuration(d.fraction());
                                          rest->setTrack(staffIdx * VOICES);
                                          Segment* seg = measure->getSegment(rest, ltick);
                                          seg->add(rest);
                                          ltick += rest->actualTicks();
                                          k++;
                                          }
                                    }
                              }
                        else {
                              if (linkedToPrevious && rest)
                                    rest = static_cast<Rest*>(rest->linkedClone());
                              else
                                    rest = new Rest(score, TDuration(TDuration::DurationType::V_MEASURE));
                              rest->setScore(_score);
                              rest->setDuration(measure->len());
                              rest->setTrack(staffIdx * VOICES);
                              Segment* seg = measure->getSegment(rest, tick);
                              seg->add(rest);
                              }
                        }
                  }
            }
//TODO      score->lastMeasure()->setEndBarLineType(BarLineType::END, false);

      //
      // select first rest
      //
      Measure* m = score->firstMeasure();
      for (Segment* s = m->first(); s; s = s->next()) {
            if (s->segmentType() == Segment::Type::ChordRest) {
                  if (s->element(0)) {
                        score->select(s->element(0), SelectType::SINGLE, 0);
                        break;
                        }
                  }
            }

      QString title     = newWizard->title();
      QString subtitle  = newWizard->subtitle();
      QString composer  = newWizard->composer();
      QString poet      = newWizard->poet();
      QString copyright = newWizard->copyright();

      if (!title.isEmpty() || !subtitle.isEmpty() || !composer.isEmpty() || !poet.isEmpty()) {
            MeasureBase* measure = score->measures()->first();
            if (measure->type() != Element::Type::VBOX) {
                  MeasureBase* nm = nvb ? nvb : new VBox(score);
                  nm->setTick(0);
                  nm->setNext(measure);
                  score->measures()->add(nm);
                  measure = nm;
                  }
            else if (nvb) {
                  delete nvb;
                  }
            if (!title.isEmpty()) {
                  Text* s = new Text(score);
                  s->setTextStyleType(TextStyleType::TITLE);
                  s->setPlainText(title);
                  measure->add(s);
                  score->setMetaTag("workTitle", title);
                  }
            if (!subtitle.isEmpty()) {
                  Text* s = new Text(score);
                  s->setTextStyleType(TextStyleType::SUBTITLE);
                  s->setPlainText(subtitle);
                  measure->add(s);
                  }
            if (!composer.isEmpty()) {
                  Text* s = new Text(score);
                  s->setTextStyleType(TextStyleType::COMPOSER);
                  s->setPlainText(composer);
                  measure->add(s);
                  score->setMetaTag("composer", composer);
                  }
            if (!poet.isEmpty()) {
                  Text* s = new Text(score);
                  s->setTextStyleType(TextStyleType::POET);
                  s->setPlainText(poet);
                  measure->add(s);
                  // the poet() functions returns data called lyricist in the dialog
                  score->setMetaTag("lyricist", poet);
                  }
            }
      else if (nvb) {
            delete nvb;
            }

      if (newWizard->createTempo()) {
            double tempo = newWizard->tempo();
            TempoText* tt = new TempoText(score);
            tt->setXmlText(QString("<sym>metNoteQuarterUp</sym> = %1").arg(tempo));
            tempo /= 60;      // bpm -> bps

            tt->setTempo(tempo);
            tt->setFollowText(true);
            tt->setTrack(0);
            Segment* seg = score->firstMeasure()->first(Segment::Type::ChordRest);
            seg->add(tt);
            score->setTempo(0, tempo);
            }
      if (!copyright.isEmpty())
            score->setMetaTag("copyright", copyright);

      score->rebuildMidiMapping();
      score->doLayout();
      setCurrentScoreView(appendScore(score));

      for (Excerpt* x : excerpts) {
            Score* xs = new Score(static_cast<MasterScore*>(score));
            xs->setName(x->title());
            xs->style()->set(StyleIdx::createMultiMeasureRests, true);
            x->setPartScore(xs);
            score->excerpts().append(x);
            createExcerpt(x);
            score->setExcerptsChanged(true);
            }
      }

//---------------------------------------------------------
//   addScorePreview
//    add a score preview to the file dialog
//---------------------------------------------------------

static void addScorePreview(QFileDialog* dialog)
      {
      QSplitter* splitter = dialog->findChild<QSplitter*>("splitter");
      if (splitter) {
            ScorePreview* preview = new ScorePreview;
            splitter->addWidget(preview);
            dialog->connect(dialog, SIGNAL(currentChanged(const QString&)), preview, SLOT(setScore(const QString&)));
            }
      }

//---------------------------------------------------------
//   sidebarUrls
//    return a list of standard file dialog sidebar urls
//---------------------------------------------------------

static QList<QUrl> sidebarUrls()
      {
      QList<QUrl> urls;
      urls.append(QUrl::fromLocalFile(QDir::homePath()));
      QFileInfo myScores(preferences.myScoresPath);
      urls.append(QUrl::fromLocalFile(myScores.absoluteFilePath()));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));
      return urls;
      }

//---------------------------------------------------------
//   getOpenScoreNames
//---------------------------------------------------------

QStringList MuseScore::getOpenScoreNames(const QString& filter, const QString& title)
      {
      QSettings settings;
      QString dir = settings.value("lastOpenPath", preferences.myScoresPath).toString();
      if (preferences.nativeDialogs) {
            QStringList fileList = QFileDialog::getOpenFileNames(this,
               title, dir, filter);
            if (fileList.count() > 0) {
                  QFileInfo fi(fileList[0]);
                  settings.setValue("lastOpenPath", fi.absolutePath());
                  }
            return fileList;
            }
      QFileInfo myScores(preferences.myScoresPath);
      if (myScores.isRelative())
            myScores.setFile(QDir::home(), preferences.myScoresPath);

      if (loadScoreDialog == 0) {
            loadScoreDialog = new QFileDialog(this);
            loadScoreDialog->setFileMode(QFileDialog::ExistingFiles);
            loadScoreDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            loadScoreDialog->setWindowTitle(title);
            addScorePreview(loadScoreDialog);

            // setup side bar urls
            QList<QUrl> urls = sidebarUrls();
            urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/demos"));
            loadScoreDialog->setSidebarUrls(urls);

            loadScoreDialog->setNameFilter(filter);
            restoreDialogState("loadScoreDialog", loadScoreDialog);
            loadScoreDialog->setAcceptMode(QFileDialog::AcceptOpen);
            loadScoreDialog->setDirectory(dir);
            }
      else {
            // dialog already exists, but set title and filter
            loadScoreDialog->setWindowTitle(title);
            loadScoreDialog->setNameFilter(filter);
            }

      QStringList result;
      if (loadScoreDialog->exec())
            result = loadScoreDialog->selectedFiles();
      settings.setValue("lastOpenPath", loadScoreDialog->directory().absolutePath());
      return result;
      }

//---------------------------------------------------------
//   getSaveScoreName
//---------------------------------------------------------

QString MuseScore::getSaveScoreName(const QString& title,
   QString& name, const QString& filter, bool selectFolder, QString* selectedFilter)
      {
      QFileInfo myName(name);
      if (myName.isRelative())
            myName.setFile(QDir::home(), name);
      name = myName.absoluteFilePath();

      if (preferences.nativeDialogs) {
            QString rv;
            QFileDialog dialog(this, title, myName.absolutePath(), filter);
            dialog.selectFile(myName.fileName());
            dialog.setAcceptMode(QFileDialog::AcceptSave);
            QFileDialog::Options options = selectFolder
                                         ? QFileDialog::ShowDirsOnly
                                         : QFileDialog::Options(0);
            dialog.setOptions(options);
            if(dialog.exec()) {
                rv = dialog.selectedFiles()[0];
                if (selectedFilter != 0)
                    *selectedFilter = dialog.selectedNameFilter();
                }
            return rv;
            }

      QFileInfo myScores(preferences.myScoresPath);
      if (myScores.isRelative())
            myScores.setFile(QDir::home(), preferences.myScoresPath);
      if (saveScoreDialog == 0) {
            saveScoreDialog = new QFileDialog(this);
            saveScoreDialog->setFileMode(QFileDialog::AnyFile);
            saveScoreDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
            saveScoreDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            saveScoreDialog->setAcceptMode(QFileDialog::AcceptSave);
            addScorePreview(saveScoreDialog);

            // setup side bar urls
            saveScoreDialog->setSidebarUrls(sidebarUrls());

            restoreDialogState("saveScoreDialog", saveScoreDialog);
            }
      if (selectFolder)
            saveScoreDialog->setFileMode(QFileDialog::Directory);

      saveScoreDialog->setWindowTitle(title);
      saveScoreDialog->setNameFilter(filter);
      saveScoreDialog->selectFile(name);

      if (!selectFolder) {
            connect(saveScoreDialog, SIGNAL(filterSelected(const QString&)),
               SLOT(saveScoreDialogFilterSelected(const QString&)));
            }
      QString s;
      if (saveScoreDialog->exec())
            s = saveScoreDialog->selectedFiles().front();
      return s;
      }

//---------------------------------------------------------
//   saveScoreDialogFilterSelected
//    update selected file name extensions, when filter
//    has changed
//---------------------------------------------------------

void MuseScore::saveScoreDialogFilterSelected(const QString& s)
      {
      QRegExp rx(QString(".+\\(\\*\\.(.+)\\)"));
      if (rx.exactMatch(s)) {
            QFileInfo fi(saveScoreDialog->selectedFiles().front());
            saveScoreDialog->selectFile(fi.completeBaseName() + "." + rx.cap(1));
            }
      }

//---------------------------------------------------------
//   getStyleFilename
//---------------------------------------------------------

QString MuseScore::getStyleFilename(bool open, const QString& title)
      {
      QFileInfo myStyles(preferences.myStylesPath);
      if (myStyles.isRelative())
            myStyles.setFile(QDir::home(), preferences.myStylesPath);
      QString defaultPath = myStyles.absoluteFilePath();

      if (preferences.nativeDialogs) {
            QString fn;
            if (open) {
                  fn = QFileDialog::getOpenFileName(
                     this, tr("MuseScore: Load Style"),
                     defaultPath,
                     tr("MuseScore Styles") + " (*.mss)"
                     );
                  }
            else {
                  fn = QFileDialog::getSaveFileName(
                     this, tr("MuseScore: Save Style"),
                     defaultPath,
                     tr("MuseScore Style File") + " (*.mss)"
                     );
                  }
            return fn;
            }

      QFileDialog* dialog;
      QList<QUrl> urls;
      QString home = QDir::homePath();
      urls.append(QUrl::fromLocalFile(home));
      urls.append(QUrl::fromLocalFile(defaultPath));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));

      if (open) {
            if (loadStyleDialog == 0) {
                  loadStyleDialog = new QFileDialog(this);
                  loadStyleDialog->setFileMode(QFileDialog::ExistingFile);
                  loadStyleDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  loadStyleDialog->setWindowTitle(title.isEmpty() ? tr("MuseScore: Load Style") : title);
                  loadStyleDialog->setNameFilter(tr("MuseScore Style File") + " (*.mss)");
                  loadStyleDialog->setDirectory(defaultPath);

                  restoreDialogState("loadStyleDialog", loadStyleDialog);
                  loadStyleDialog->setAcceptMode(QFileDialog::AcceptOpen);
                  }
            urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/styles"));
            dialog = loadStyleDialog;
            }
      else {
            if (saveStyleDialog == 0) {
                  saveStyleDialog = new QFileDialog(this);
                  saveStyleDialog->setAcceptMode(QFileDialog::AcceptSave);
                  saveStyleDialog->setFileMode(QFileDialog::AnyFile);
                  saveStyleDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
                  saveStyleDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  saveStyleDialog->setWindowTitle(title.isEmpty() ? tr("MuseScore: Save Style") : title);
                  saveStyleDialog->setNameFilter(tr("MuseScore Style File") + " (*.mss)");
                  saveStyleDialog->setDirectory(defaultPath);

                  restoreDialogState("saveStyleDialog", saveStyleDialog);
                  saveStyleDialog->setAcceptMode(QFileDialog::AcceptSave);
                  }
            dialog = saveStyleDialog;
            }
      // setup side bar urls
      dialog->setSidebarUrls(urls);

      if (dialog->exec()) {
            QStringList result = dialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   getChordStyleFilename
//---------------------------------------------------------

QString MuseScore::getChordStyleFilename(bool open)
      {
      QString filter = tr("Chord Symbols Style File") + " (*.xml)";

      QFileInfo myStyles(preferences.myStylesPath);
      if (myStyles.isRelative())
            myStyles.setFile(QDir::home(), preferences.myStylesPath);
      QString defaultPath = myStyles.absoluteFilePath();

      if (preferences.nativeDialogs) {
            QString fn;
            if (open) {
                  fn = QFileDialog::getOpenFileName(
                     this, tr("MuseScore: Load Chord Symbols Style"),
                     defaultPath,
                     filter
                     );
                  }
            else {
                  fn = QFileDialog::getSaveFileName(
                     this, tr("MuseScore: Save Chord Symbols Style"),
                     defaultPath,
                     filter
                     );
                  }
            return fn;
            }

      QFileDialog* dialog;
      QList<QUrl> urls;
      QString home = QDir::homePath();
      urls.append(QUrl::fromLocalFile(home));
      urls.append(QUrl::fromLocalFile(defaultPath));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));

      QSettings settings;
      if (open) {
            if (loadChordStyleDialog == 0) {
                  loadChordStyleDialog = new QFileDialog(this);
                  loadChordStyleDialog->setFileMode(QFileDialog::ExistingFile);
                  loadChordStyleDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  loadChordStyleDialog->setWindowTitle(tr("MuseScore: Load Chord Symbols Style"));
                  loadChordStyleDialog->setNameFilter(filter);
                  loadChordStyleDialog->setDirectory(defaultPath);

                  restoreDialogState("loadChordStyleDialog", loadChordStyleDialog);
                  loadChordStyleDialog->restoreState(settings.value("loadChordStyleDialog").toByteArray());
                  loadChordStyleDialog->setAcceptMode(QFileDialog::AcceptOpen);
                  }
            // setup side bar urls
            urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/styles"));
            dialog = loadChordStyleDialog;
            }
      else {
            if (saveChordStyleDialog == 0) {
                  saveChordStyleDialog = new QFileDialog(this);
                  saveChordStyleDialog->setAcceptMode(QFileDialog::AcceptSave);
                  saveChordStyleDialog->setFileMode(QFileDialog::AnyFile);
                  saveChordStyleDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
                  saveChordStyleDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  saveChordStyleDialog->setWindowTitle(tr("MuseScore: Save Style"));
                  saveChordStyleDialog->setNameFilter(filter);
                  saveChordStyleDialog->setDirectory(defaultPath);

                  restoreDialogState("saveChordStyleDialog", saveChordStyleDialog);
                  saveChordStyleDialog->setAcceptMode(QFileDialog::AcceptSave);
                  }
            dialog = saveChordStyleDialog;
            }
      // setup side bar urls
      dialog->setSidebarUrls(urls);
      if (dialog->exec()) {
            QStringList result = dialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   getScanFile
//---------------------------------------------------------

QString MuseScore::getScanFile(const QString& d)
      {
      QString filter = tr("PDF Scan File") + " (*.pdf);;All (*)";
      QString defaultPath = d.isEmpty() ? QDir::homePath() : d;
      if (preferences.nativeDialogs) {
            QString s = QFileDialog::getOpenFileName(
               mscore,
               MuseScore::tr("Choose PDF Scan"),
               defaultPath,
               filter
               );
            return s;
            }

      if (loadScanDialog == 0) {
            loadScanDialog = new QFileDialog(this);
            loadScanDialog->setFileMode(QFileDialog::ExistingFile);
            loadScanDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            loadScanDialog->setWindowTitle(tr("MuseScore: Choose PDF Scan"));
            loadScanDialog->setNameFilter(filter);
            loadScanDialog->setDirectory(defaultPath);

            restoreDialogState("loadScanDialog", loadScanDialog);
            loadScanDialog->setAcceptMode(QFileDialog::AcceptOpen);
            }

      //
      // setup side bar urls
      //
      QList<QUrl> urls;
      QString home = QDir::homePath();
      urls.append(QUrl::fromLocalFile(home));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));
      loadScanDialog->setSidebarUrls(urls);

      if (loadScanDialog->exec()) {
            QStringList result = loadScanDialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   getAudioFile
//---------------------------------------------------------

QString MuseScore::getAudioFile(const QString& d)
      {
      QString filter = tr("Ogg Audio File") + " (*.ogg);;All (*)";
      QString defaultPath = d.isEmpty() ? QDir::homePath() : d;
      if (preferences.nativeDialogs) {
            QString s = QFileDialog::getOpenFileName(
               mscore,
               MuseScore::tr("Choose Audio File"),
               defaultPath,
               filter
               );
            return s;
            }

      if (loadAudioDialog == 0) {
            loadAudioDialog = new QFileDialog(this);
            loadAudioDialog->setFileMode(QFileDialog::ExistingFile);
            loadAudioDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            loadAudioDialog->setWindowTitle(tr("MuseScore: Choose Ogg Audio File"));
            loadAudioDialog->setNameFilter(filter);
            loadAudioDialog->setDirectory(defaultPath);

            restoreDialogState("loadAudioDialog", loadAudioDialog);
            loadAudioDialog->setAcceptMode(QFileDialog::AcceptOpen);
            }

      //
      // setup side bar urls
      //
      QList<QUrl> urls;
      QString home = QDir::homePath();
      urls.append(QUrl::fromLocalFile(home));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));
      loadAudioDialog->setSidebarUrls(urls);

      if (loadAudioDialog->exec()) {
            QStringList result = loadAudioDialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   getFotoFilename
//---------------------------------------------------------

QString MuseScore::getFotoFilename(QString& filter, QString* selectedFilter)
      {
      QString title = tr("MuseScore: Save Image");

      QFileInfo myImages(preferences.myImagesPath);
      if (myImages.isRelative())
            myImages.setFile(QDir::home(), preferences.myImagesPath);
      QString defaultPath = myImages.absoluteFilePath();

      if (preferences.nativeDialogs) {
            QString fn;
            fn = QFileDialog::getSaveFileName(
               this,
               title,
               defaultPath,
               filter,
               selectedFilter
               );
            return fn;
            }


      QList<QUrl> urls;
      urls.append(QUrl::fromLocalFile(QDir::homePath()));
      urls.append(QUrl::fromLocalFile(defaultPath));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));

      if (saveImageDialog == 0) {
            saveImageDialog = new QFileDialog(this);
            saveImageDialog->setFileMode(QFileDialog::AnyFile);
            saveImageDialog->setAcceptMode(QFileDialog::AcceptSave);
            saveImageDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
            saveImageDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            saveImageDialog->setWindowTitle(title);
            saveImageDialog->setNameFilter(filter);
            saveImageDialog->setDirectory(defaultPath);

            restoreDialogState("saveImageDialog", saveImageDialog);
            saveImageDialog->setAcceptMode(QFileDialog::AcceptSave);
            }

      // setup side bar urls
      saveImageDialog->setSidebarUrls(urls);

      if (saveImageDialog->exec()) {
            QStringList result = saveImageDialog->selectedFiles();
            *selectedFilter = saveImageDialog->selectedNameFilter();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   getPaletteFilename
//---------------------------------------------------------

QString MuseScore::getPaletteFilename(bool open, const QString& name)
      {
      QString title;
      QString filter;
      QString wd      = QString("%1/%2").arg(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).arg(QCoreApplication::applicationName());
      if (open) {
            title  = tr("MuseScore: Load Palette");
            filter = tr("MuseScore Palette") + " (*.mpal)";
            }
      else {
            title  = tr("MuseScore: Save Palette");
            filter = tr("MuseScore Palette") + " (*.mpal)";
            }

      QFileInfo myPalettes(wd);
      QString defaultPath = myPalettes.absoluteFilePath();
      if (!name.isEmpty()) {
            QString fname = createDefaultFileName(name);
            QFileInfo myName(fname);
            if (myName.isRelative())
                  myName.setFile(defaultPath, fname);
            defaultPath = myName.absoluteFilePath();
            }

      if (preferences.nativeDialogs) {
            QString fn;
            if (open)
                  fn = QFileDialog::getOpenFileName(this, title, defaultPath, filter);
            else
                  fn = QFileDialog::getSaveFileName(this, title, defaultPath, filter);
            return fn;
            }

      QFileDialog* dialog;
      QList<QUrl> urls;
      urls.append(QUrl::fromLocalFile(QDir::homePath()));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));
      urls.append(QUrl::fromLocalFile(defaultPath));

      if (open) {
            if (loadPaletteDialog == 0) {
                  loadPaletteDialog = new QFileDialog(this);
                  loadPaletteDialog->setFileMode(QFileDialog::ExistingFile);
                  loadPaletteDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  loadPaletteDialog->setDirectory(defaultPath);

                  restoreDialogState("loadPaletteDialog", loadPaletteDialog);
                  loadPaletteDialog->setAcceptMode(QFileDialog::AcceptOpen);
                  }
            urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/styles"));
            dialog = loadPaletteDialog;
            }
      else {
            if (savePaletteDialog == 0) {
                  savePaletteDialog = new QFileDialog(this);
                  savePaletteDialog->setAcceptMode(QFileDialog::AcceptSave);
                  savePaletteDialog->setFileMode(QFileDialog::AnyFile);
                  savePaletteDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
                  savePaletteDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  savePaletteDialog->setDirectory(defaultPath);

                  restoreDialogState("savePaletteDialog", savePaletteDialog);
                  savePaletteDialog->setAcceptMode(QFileDialog::AcceptSave);
                  }
            dialog = savePaletteDialog;
            }
      dialog->setWindowTitle(title);
      dialog->setNameFilter(filter);

      // setup side bar urls
      dialog->setSidebarUrls(urls);

      if (dialog->exec()) {
            QStringList result = dialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   getPluginFilename
//---------------------------------------------------------

QString MuseScore::getPluginFilename(bool open)
      {
      QString title;
      QString filter;
      if (open) {
            title  = tr("MuseScore: Load Plugin");
            filter = tr("MuseScore Plugin") + " (*.qml)";
            }
      else {
            title  = tr("MuseScore: Save Plugin");
            filter = tr("MuseScore Plugin File") + " (*.qml)";
            }

      QFileInfo myPlugins(preferences.myPluginsPath);
      if (myPlugins.isRelative())
            myPlugins.setFile(QDir::home(), preferences.myPluginsPath);
      QString defaultPath = myPlugins.absoluteFilePath();

      QString name  = createDefaultFileName("Plugin");
      QString fname = QString("%1/%2.qml").arg(defaultPath).arg(name);
      if (preferences.nativeDialogs) {
            QString fn;
            if (open)
                  fn = QFileDialog::getOpenFileName(this, title, defaultPath, filter);
            else
                  fn = QFileDialog::getSaveFileName(this, title, defaultPath, filter);
            return fn;
            }

      QFileDialog* dialog;
      QList<QUrl> urls;
      QString home = QDir::homePath();
      urls.append(QUrl::fromLocalFile(home));
      urls.append(QUrl::fromLocalFile(defaultPath));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));

      if (open) {
            if (loadPluginDialog == 0) {
                  loadPluginDialog = new QFileDialog(this);
                  loadPluginDialog->setFileMode(QFileDialog::ExistingFile);
                  loadPluginDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  loadPluginDialog->setDirectory(defaultPath);

                  QSettings settings;
                  loadPluginDialog->restoreState(settings.value("loadPluginDialog").toByteArray());
                  loadPluginDialog->setAcceptMode(QFileDialog::AcceptOpen);
                  }
            urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/styles"));
            dialog = loadPluginDialog;
            }
      else {
            if (savePluginDialog == 0) {
                  savePluginDialog = new QFileDialog(this);
                  QSettings settings;
                  savePluginDialog->restoreState(settings.value("savePluginDialog").toByteArray());
                  savePluginDialog->setAcceptMode(QFileDialog::AcceptSave);
                  savePluginDialog->setFileMode(QFileDialog::AnyFile);
                  savePluginDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
                  savePluginDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  savePluginDialog->setWindowTitle(tr("MuseScore: Save Plugin"));
                  savePluginDialog->setNameFilter(filter);
                  savePluginDialog->setDirectory(defaultPath);
                  savePluginDialog->selectFile(fname);
                  }
            dialog = savePluginDialog;
            }
      dialog->setWindowTitle(title);
      dialog->setNameFilter(filter);

      // setup side bar urls
      dialog->setSidebarUrls(urls);

      if (dialog->exec()) {
            QStringList result = dialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   getDrumsetFilename
//---------------------------------------------------------

QString MuseScore::getDrumsetFilename(bool open)
      {
      QString title;
      QString filter;
      if (open) {
            title  = tr("MuseScore: Load Drumset");
            filter = tr("MuseScore Drumset") + " (*.drm)";
            }
      else {
            title  = tr("MuseScore: Save Drumset");
            filter = tr("MuseScore Drumset File") + " (*.drm)";
            }

      QFileInfo myStyles(preferences.myStylesPath);
      if (myStyles.isRelative())
            myStyles.setFile(QDir::home(), preferences.myStylesPath);
      QString defaultPath  = myStyles.absoluteFilePath();

      if (preferences.nativeDialogs) {
            QString fn;
            if (open)
                  fn = QFileDialog::getOpenFileName(this, title, defaultPath, filter);
            else
                  fn = QFileDialog::getSaveFileName(this, title, defaultPath, filter);
            return fn;
            }


      QFileDialog* dialog;
      QList<QUrl> urls;
      QString home = QDir::homePath();
      urls.append(QUrl::fromLocalFile(home));
      urls.append(QUrl::fromLocalFile(defaultPath));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));

      if (open) {
            if (loadDrumsetDialog == 0) {
                  loadDrumsetDialog = new QFileDialog(this);
                  loadDrumsetDialog->setFileMode(QFileDialog::ExistingFile);
                  loadDrumsetDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  loadDrumsetDialog->setDirectory(defaultPath);

                  restoreDialogState("loadDrumsetDialog", loadDrumsetDialog);
                  loadDrumsetDialog->setAcceptMode(QFileDialog::AcceptOpen);
                  }
            urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/styles"));
            dialog = loadDrumsetDialog;
            }
      else {
            if (saveDrumsetDialog == 0) {
                  saveDrumsetDialog = new QFileDialog(this);
                  saveDrumsetDialog->setAcceptMode(QFileDialog::AcceptSave);
                  saveDrumsetDialog->setFileMode(QFileDialog::AnyFile);
                  saveDrumsetDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
                  saveDrumsetDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  saveDrumsetDialog->setDirectory(defaultPath);

                  restoreDialogState("saveDrumsetDialog", saveDrumsetDialog);
                  saveDrumsetDialog->setAcceptMode(QFileDialog::AcceptSave);
                  }
            dialog = saveDrumsetDialog;
            }
      dialog->setWindowTitle(title);
      dialog->setNameFilter(filter);

      // setup side bar urls
      dialog->setSidebarUrls(urls);

      if (dialog->exec()) {
            QStringList result = dialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

//---------------------------------------------------------
//   printFile
//---------------------------------------------------------

void MuseScore::printFile()
      {
      QPrinter printerDev(QPrinter::HighResolution);
      const PageFormat* pf = cs->pageFormat();
      QPageSize ps(QPageSize::id(pf->size(), QPageSize::Point));
      printerDev.setPageSize(ps);
      printerDev.setPageOrientation(
            pf->size().width() > pf->size().height() ? QPageLayout::Landscape : QPageLayout::Portrait
         );

      printerDev.setCreator("MuseScore Version: " VERSION);
      printerDev.setFullPage(true);
      if (!printerDev.setPageMargins(QMarginsF()))
            qDebug("unable to clear printer margins");
      printerDev.setColorMode(QPrinter::Color);
      printerDev.setDocName(cs->fileInfo()->completeBaseName());
      printerDev.setOutputFormat(QPrinter::NativeFormat);
      int pages    = cs->pages().size();
      printerDev.setFromTo(1, pages);

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
      printerDev.setOutputFileName("");
#else
      // when setting this on windows platform, pd.exec() does not
      // show dialog
      printerDev.setOutputFileName(cs->masterScore()->fileInfo()->path() + "/" + cs->fileInfo()->completeBaseName() + ".pdf");
#endif

      QPrintDialog pd(&printerDev, 0);

      if (!pd.exec())
            return;

      LayoutMode layoutMode = cs->layoutMode();
      if (layoutMode != LayoutMode::PAGE) {
            cs->setLayoutMode(LayoutMode::PAGE);
            cs->doLayout();
            }

      QPainter p(&printerDev);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      double mag = printerDev.logicalDpiX() / DPI;

      p.scale(mag, mag);

      int fromPage = printerDev.fromPage() - 1;
      int toPage   = printerDev.toPage() - 1;
      if (fromPage < 0)
            fromPage = 0;
      if ((toPage < 0) || (toPage >= pages))
            toPage = pages - 1;

      for (int copy = 0; copy < printerDev.numCopies(); ++copy) {
            bool firstPage = true;
            for (int n = fromPage; n <= toPage; ++n) {
                  if (!firstPage)
                        printerDev.newPage();
                  firstPage = false;

                  cs->print(&p, n);
                  if ((copy + 1) < printerDev.numCopies())
                        printerDev.newPage();
                  }
            }
      p.end();
      if (layoutMode != cs->layoutMode()) {
            cs->setLayoutMode(layoutMode);
            cs->doLayout();
            }
      }

//---------------------------------------------------------
//   exportFile
//    return true on success
//---------------------------------------------------------

void MuseScore::exportFile()
      {
      QStringList fl;
      fl.append(tr("PDF File") + " (*.pdf)");
      fl.append(tr("PNG Bitmap Graphic") + " (*.png)");
      fl.append(tr("Scalable Vector Graphic") + " (*" + EXT_SVG + ")");
#ifdef HAS_AUDIOFILE
      fl.append(tr("Wave Audio") + " (*.wav)");
      fl.append(tr("FLAC Audio") + " (*.flac)");
      fl.append(tr("Ogg Vorbis Audio") + " (*.ogg)");
#endif
#ifdef USE_LAME
      fl.append(tr("MP3 Audio") + " (*.mp3)");
#endif
      fl.append(tr("Standard MIDI File") + " (*.mid)");
      fl.append(tr("MusicXML File") + " (*.xml)");
      fl.append(tr("Compressed MusicXML File") + " (*.mxl)");
      fl.append(tr("Uncompressed MuseScore File") + " (*.mscx)");
// SMAWS options
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_AUTO_OPEN).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_AUTO_ALL).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_MULTI).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_RULERS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_GRID).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_GRID_RULERS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_TABLES).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_FRETS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_MIX_TREE).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_LYRICS).arg(EXT_VTT));
// SMAWS end

      QString saveDialogTitle = tr("MuseScore: Export");

      QSettings settings;
      if (lastSaveCopyDirectory.isEmpty())
            lastSaveCopyDirectory = settings.value("lastSaveCopyDirectory", preferences.myScoresPath).toString();
      if (lastSaveDirectory.isEmpty())
            lastSaveDirectory = settings.value("lastSaveDirectory", preferences.myScoresPath).toString();
      QString saveDirectory = lastSaveCopyDirectory;

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.myScoresPath;

      if (lastSaveCopyFormat.isEmpty())
            lastSaveCopyFormat = settings.value("lastSaveCopyFormat", "pdf").toString();
      QString saveFormat = lastSaveCopyFormat;

      if (saveFormat.isEmpty())
            saveFormat = "pdf";

      QString name;
#ifdef Q_OS_WIN
      if (QSysInfo::WindowsVersion == QSysInfo::WV_XP) {
            if (!cs->isMaster())
                  name = QString("%1/%2-%3").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->name()));
            else
                  name = QString("%1/%2").arg(saveDirectory).arg(cs->fileInfo()->completeBaseName());
            }
      else
#endif
      if (!cs->isMaster())
            name = QString("%1/%2-%3.%4").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->fileInfo()->completeBaseName())).arg(saveFormat);
      else
            name = QString("%1/%2.%3").arg(saveDirectory).arg(cs->fileInfo()->completeBaseName()).arg(saveFormat);

      int idx = fl.indexOf(QRegExp(".+\\(\\*\\." + saveFormat + "\\)"), Qt::CaseInsensitive);
      if (idx != -1)
            fl.move(idx, 0);
      QString filter = fl.join(";;");
      QString selectedFilter;
      QString fn = getSaveScoreName(saveDialogTitle, name, filter, false, &selectedFilter);
      if (fn.isEmpty())
            return;

      QFileInfo fi(fn);
      lastSaveCopyDirectory = fi.absolutePath();
      lastSaveCopyFormat = fi.suffix();

      // SaveAs() is restrictive in a variety of ways, especially the lack of
      // selectedFilter. For SMAWS, this is all that's necessary:
      if (fn.right(4) == EXT_VTT) {
            if (cs->layoutMode() != LayoutMode::PAGE) {
                cs->setLayoutMode(LayoutMode::PAGE);
                cs->doLayout();
            }

            if (selectedFilter.contains(FILTER_SMAWS_AUTO_OPEN))
                  autoSMAWS(cs, &fi, false);
            if (selectedFilter.contains(FILTER_SMAWS_AUTO_ALL))
                  autoSMAWS(cs, &fi, true);
            else if (selectedFilter.contains(FILTER_SMAWS))
                  saveSMAWS_Music(cs, &fi, false, true); // for PART, auto-export FRETS
            else if (selectedFilter.contains(FILTER_SMAWS_MULTI))
                  saveSMAWS_Music(cs, &fi, true, false); // for SCORE, don't export FRETS
            else if (selectedFilter.contains(FILTER_SMAWS_RULERS))
                  saveSMAWS_Rulers(cs, &fi);
            else if (selectedFilter.contains(FILTER_SMAWS_GRID))
                  saveSMAWS_Tables(cs, &fi, false, false);
            else if (selectedFilter.contains(FILTER_SMAWS_GRID_RULERS))
                  saveSMAWS_Tables(cs, &fi, false, true);
            else if (selectedFilter.contains(FILTER_SMAWS_TABLES))
                  saveSMAWS_Tables(cs, &fi, true, false);
            else if (selectedFilter.contains(FILTER_SMAWS_FRETS))
                  saveSMAWS_Frets(cs, &fi);
            else if (selectedFilter.contains(FILTER_SMAWS_MIX_TREE))
                  saveSMAWS_Tree(cs, &fi);
            else if (selectedFilter.contains(FILTER_SMAWS_LYRICS))
                  saveSMAWS_Lyrics(cs, &fi);
      }

      else if (fi.suffix().isEmpty())
            QMessageBox::critical(this, tr("MuseScore: Export"), tr("Cannot determine file type"));
      else // Everything NOT SMAWS
            saveAs(cs, true, fn, lastSaveCopyFormat);
      }

//---------------------------------------------------------
//   exportParts
//    return true on success
//---------------------------------------------------------

bool MuseScore::exportParts()
      {
      QStringList fl;
      fl.append(tr("PDF File") + " (*.pdf)");
      fl.append(tr("PNG Bitmap Graphic") + " (*.png)");
      fl.append(tr("Scalable Vector Graphic") + " (*" + EXT_SVG + ")");
#ifdef HAS_AUDIOFILE
      fl.append(tr("Wave Audio") + " (*.wav)");
      fl.append(tr("FLAC Audio") + " (*.flac)");
      fl.append(tr("Ogg Vorbis Audio") + " (*.ogg)");
#endif
#ifdef USE_LAME
      fl.append(tr("MP3 Audio") + " (*.mp3)");
#endif
      fl.append(tr("Standard MIDI File") + " (*.mid)");
      fl.append(tr("MusicXML File") + " (*.xml)");
      fl.append(tr("Compressed MusicXML File") + " (*.mxl)");
      fl.append(tr("MuseScore File") + " (*.mscz)");
      fl.append(tr("Uncompressed MuseScore File") + " (*.mscx)");

      QString saveDialogTitle = tr("MuseScore: Export Parts");

      QSettings settings;
      if (lastSaveCopyDirectory.isEmpty())
          lastSaveCopyDirectory = settings.value("lastSaveCopyDirectory", preferences.myScoresPath).toString();
      if (lastSaveDirectory.isEmpty())
          lastSaveDirectory = settings.value("lastSaveDirectory", preferences.myScoresPath).toString();
      QString saveDirectory = lastSaveCopyDirectory;

      if (saveDirectory.isEmpty()) {
          saveDirectory = preferences.myScoresPath;
          }

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.myScoresPath;

      if (lastSaveCopyFormat.isEmpty())
            lastSaveCopyFormat = settings.value("lastSaveCopyFormat", "pdf").toString();
      QString saveFormat = lastSaveCopyFormat;

      if (saveFormat.isEmpty())
            saveFormat = "pdf";

      QString scoreName = (cs->isMaster() ? cs : cs->masterScore())->fileInfo()->completeBaseName();
      QString name;
#ifdef Q_OS_WIN
      if (QSysInfo::WindowsVersion == QSysInfo::WV_XP)
            name = QString("%1/%2").arg(saveDirectory).arg(scoreName);
      else
#endif
      name = QString("%1/%2.%3").arg(saveDirectory).arg(scoreName).arg(saveFormat);

      int idx = fl.indexOf(QRegExp(".+\\(\\*\\." + saveFormat + "\\)"), Qt::CaseInsensitive);
      if (idx != -1)
            fl.move(idx, 0);
      QString filter = fl.join(";;");
      QString fn = getSaveScoreName(saveDialogTitle, name, filter);
      if (fn.isEmpty())
          return false;

      QFileInfo fi(fn);
      lastSaveCopyDirectory = fi.absolutePath();
      lastSaveCopyFormat = fi.suffix();

      QString ext = fi.suffix();
      if (ext.isEmpty()) {
            QMessageBox::critical(this, tr("MuseScore: Export Parts"), tr("Cannot determine file type"));
            return false;
            }

      Score* thisScore = cs->masterScore();
      bool overwrite = false;
      bool noToAll = false;
      QString confirmReplaceTitle = tr("Confirm Replace");
      QString confirmReplaceMessage = tr("\"%1\" already exists.\nDo you want to replace it?\n");
      QString replaceMessage = tr("Replace");
      QString skipMessage = tr("Skip");
      foreach (Excerpt* e, thisScore->excerpts())  {
            Score* pScore = e->partScore();
            QString partfn = fi.absolutePath() + QDir::separator() + fi.completeBaseName() + "-" + createDefaultFileName(pScore->fileInfo()->completeBaseName()) + "." + ext;
            QFileInfo fip(partfn);
            if(fip.exists() && !overwrite) {
                  if(noToAll)
                        continue;
                  QMessageBox msgBox( QMessageBox::Question, confirmReplaceTitle,
                        confirmReplaceMessage.arg(QDir::toNativeSeparators(partfn)),
                        QMessageBox::Yes |  QMessageBox::YesToAll | QMessageBox::No |  QMessageBox::NoToAll);
                  msgBox.setButtonText(QMessageBox::Yes, replaceMessage);
                  msgBox.setButtonText(QMessageBox::No, skipMessage);
                  msgBox.setButtonText(QMessageBox::YesToAll, tr("Replace All"));
                  msgBox.setButtonText(QMessageBox::NoToAll, tr("Skip All"));
                  int sb = msgBox.exec();
                  if(sb == QMessageBox::YesToAll) {
                        overwrite = true;
                        }
                  else if (sb == QMessageBox::NoToAll) {
                        noToAll = true;
                        continue;
                        }
                  else if (sb == QMessageBox::No)
                        continue;
                  }

            if (!saveAs(pScore, true, partfn, ext))
                  return false;
            }
      // For PDF, also export score and parts together
      if (ext.toLower() == "pdf") {
            QList<Score*> scores;
            scores.append(thisScore);
            foreach(Excerpt* e, thisScore->excerpts())  {
                  scores.append(e->partScore());
                  }
            QString partfn(fi.absolutePath() + QDir::separator() + fi.completeBaseName() + "-" + createDefaultFileName(tr("Score_and_Parts")) + ".pdf");
            QFileInfo fip(partfn);
            if(fip.exists() && !overwrite) {
                  if (!noToAll) {
                        QMessageBox msgBox( QMessageBox::Question, confirmReplaceTitle,
                              confirmReplaceMessage.arg(QDir::toNativeSeparators(partfn)),
                              QMessageBox::Yes | QMessageBox::No);
                        msgBox.setButtonText(QMessageBox::Yes, replaceMessage);
                        msgBox.setButtonText(QMessageBox::No, skipMessage);
                        int sb = msgBox.exec();
                        if(sb == QMessageBox::Yes) {
                              if (!savePdf(scores, partfn))
                                    return false;
                              }
                        }
                  }
            else if (!savePdf(scores, partfn))
                  return false;
      }
      if(!noToAll)
            QMessageBox::information(this, tr("MuseScore: Export Parts"), tr("Parts were successfully exported"));
      return true;
      }

//---------------------------------------------------------
//   saveAs
//---------------------------------------------------------

bool MuseScore::saveAs(Score* cs, bool saveCopy, const QString& path, const QString& ext)
      {
      bool rv = false;
      QString suffix = "." + ext;
      QString fn(path);
      if (!fn.endsWith(suffix))
            fn += suffix;

      LayoutMode layoutMode = cs->layoutMode();
      if (layoutMode != LayoutMode::PAGE) {
            cs->setLayoutMode(LayoutMode::PAGE);
            cs->doLayout();
            }
      if (ext == "mscx" || ext == "mscz") {
            // save as mscore *.msc[xz] file
            QFileInfo fi(fn);
            rv = true;
            // store new file and path into score fileInfo
            // to have it accessible to resources
            QString originalScoreFName(cs->masterScore()->fileInfo()->canonicalFilePath());
            cs->masterScore()->fileInfo()->setFile(fn);
            try {
                  if (ext == "mscz")
                        cs->saveCompressedFile(fi, false);
                  else
                        cs->saveFile(fi);
                  }
            catch (QString s) {
                  rv = false;
                  QMessageBox::critical(this, tr("MuseScore: Save As"), s);
                  }
            cs->masterScore()->fileInfo()->setFile(originalScoreFName);          // restore original file name

            if (rv && !saveCopy) {
                  cs->masterScore()->fileInfo()->setFile(fn);
                  setWindowTitle("MuseScore: " + cs->fileInfo()->completeBaseName());
                  cs->undoStack()->setClean();
                  dirtyChanged(cs);
                  cs->setCreated(false);
                  addRecentScore(cs);
                  writeSessionFile(false);
                  }
            }
      else if (ext == "xml") {
            // save as MusicXML *.xml file
            rv = saveXml(cs, fn);
            }
      else if (ext == "mxl") {
            // save as compressed MusicXML *.mxl file
            rv = saveMxl(cs, fn);
            }
      else if (ext == "mid") {
            // save as midi file *.mid
            rv = saveMidi(cs, fn);
            }
      else if (ext == "pdf") {
            // save as pdf file *.pdf
            rv = savePdf(cs, fn);
            }
      else if (ext == "png") {
            // save as png file *.png
            rv = savePng(cs, fn);
            }
      else if (ext == "svg") {
            // save as svg file *.svg
            rv = saveSvg(cs, fn);
            }
#ifdef HAS_AUDIOFILE
      else if (ext == "wav" || ext == "flac" || ext == "ogg")
            rv = saveAudio(cs, fn);
#endif
#ifdef USE_LAME
      else if (ext == "mp3")
            rv = saveMp3(cs, fn);
#endif
      else if (ext == "spos") {
            // save positions of segments
            rv = savePositions(cs, fn, true);
            }
      else if (ext == "mpos") {
            // save positions of measures
            rv = savePositions(cs, fn, false);
            }
      else if (ext == "mlog") {
            rv = cs->sanityCheck(fn);
            }
      else {
            qDebug("Internal error: unsupported extension <%s>",
               qPrintable(ext));
            return false;
            }
      if (!rv && !MScore::noGui)
            QMessageBox::critical(this, tr("MuseScore:"), tr("Cannot write into %1").arg(fn));
      if (layoutMode != cs->layoutMode()) {
            cs->setLayoutMode(layoutMode);
            cs->doLayout();
            }
      return rv;
      }

//---------------------------------------------------------
//   saveMidi
//---------------------------------------------------------

bool MuseScore::saveMidi(Score* score, const QString& name)
      {
      ExportMidi em(score);
      return em.write(name, preferences.midiExpandRepeats);
      }

//---------------------------------------------------------
//   savePdf
//---------------------------------------------------------

bool MuseScore::savePdf(const QString& saveName)
      {
      return savePdf(cs, saveName);
      }

bool MuseScore::savePdf(Score* cs, const QString& saveName)
      {
      cs->setPrinting(true);
      MScore::pdfPrinting = true;
      QPdfWriter printerDev(saveName);
      printerDev.setResolution(preferences.exportPdfDpi);
      const PageFormat* pf = cs->pageFormat();
      printerDev.setPageSize(QPageSize(pf->size(), QPageSize::Point));

      printerDev.setCreator("MuseScore Version: " VERSION);
      if (!printerDev.setPageMargins(QMarginsF()))
            qDebug("unable to clear printer margins");
      printerDev.setTitle(cs->fileInfo()->completeBaseName());

      QPainter p;
      if (!p.begin(&printerDev))
            return false;
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      double mag = printerDev.logicalDpiX() / DPI;
      p.scale(mag, mag);

      const QList<Page*> pl = cs->pages();
      int pages    = pl.size();
      bool firstPage = true;
      for (int n = 0; n < pages; ++n) {
            if (!firstPage)
                  printerDev.newPage();
            firstPage = false;
            cs->print(&p, n);
            }
      p.end();
      cs->setPrinting(false);
      MScore::pdfPrinting = false;
      return true;
      }

bool MuseScore::savePdf(QList<Score*> cs, const QString& saveName)
      {
      if (cs.empty())
            return false;
      Score* firstScore = cs[0];

      QPrinter printerDev(QPrinter::HighResolution);
      const PageFormat* pf = firstScore->pageFormat();
      printerDev.setPaperSize(pf->size(), QPrinter::Point);

      printerDev.setCreator("MuseScore Version: " VERSION);
      printerDev.setFullPage(true);
      if (!printerDev.setPageMargins(QMarginsF()))
            qDebug("unable to clear printer margins");
      printerDev.setColorMode(QPrinter::Color);
      printerDev.setDocName(firstScore->fileInfo()->completeBaseName());
      printerDev.setOutputFormat(QPrinter::PdfFormat);

      printerDev.setOutputFileName(saveName);

      QPainter p;
      if (!p.begin(&printerDev))
            return false;

      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      double mag = printerDev.logicalDpiX() / DPI;
      p.scale(mag, mag);

      bool firstPage = true;
      for (Score* s : cs) {
            LayoutMode layoutMode = s->layoutMode();
            if (layoutMode != LayoutMode::PAGE) {
                  s->setLayoutMode(LayoutMode::PAGE);
                  s->doLayout();
                  }
            s->setPrinting(true);
            MScore::pdfPrinting = true;

            const PageFormat* pf = s->pageFormat();
            printerDev.setPaperSize(pf->size(), QPrinter::Point);

            const QList<Page*> pl = s->pages();
            int pages    = pl.size();
            for (int n = 0; n < pages; ++n) {
                  if (!firstPage)
                        printerDev.newPage();
                  firstPage = false;
                  s->print(&p, n);
                  }
            //reset score
            s->setPrinting(false);
            MScore::pdfPrinting = false;

            if (layoutMode != s->layoutMode()) {
                  s->setLayoutMode(layoutMode);
                  s->doLayout();
                  }
            }
      p.end();
      return true;
      }

//---------------------------------------------------------
//   importSoundfont
//---------------------------------------------------------

void importSoundfont(QString name)
      {
      QFileInfo info(name);
      int ret = QMessageBox::question(0, QWidget::tr("Install SoundFont"),
            QWidget::tr("Do you want to install the SoundFont %1?").arg(info.fileName()),
             QMessageBox::Yes|QMessageBox::No, QMessageBox::NoButton);
      if (ret == QMessageBox::Yes) {
            QStringList pl = preferences.mySoundfontsPath.split(";");
            QString destPath;
            for (QString s : pl) {
                  QFileInfo dest(s);
                  if (dest.isWritable())
                        destPath = s;
                  }
            if (!destPath.isEmpty()) {
                  QString destFilePath = destPath+ "/" +info.fileName();
                  QFileInfo destFileInfo(destFilePath);
                  QFile destFile(destFilePath);
                  if (destFileInfo.exists()) {
                        int ret1 = QMessageBox::question(0, QWidget::tr("Overwrite?"),
                          QWidget::tr("%1 already exists.\nDo you want to overwrite it?").arg(destFileInfo.absoluteFilePath()),
                          QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
                        if (ret1 == QMessageBox::No)
                              return;
                        destFile.remove();
                        }
                  QFile orig(name);
                  if (orig.copy(destFilePath)) {
                        QMessageBox::information(0, QWidget::tr("SoundFont installed"), QWidget::tr("SoundFont installed. Please go to View > Synthesizer to add it and View > Mixer to choose an instrument sound."));
                        }
                  }
            }
      }

//---------------------------------------------------------
//   readScore
///   Import file \a name
//---------------------------------------------------------

Score::FileError readScore(MasterScore* score, QString name, bool ignoreVersionError)
      {
      QFileInfo info(name);
      QString suffix  = info.suffix().toLower();
      score->setName(info.completeBaseName());
      score->setImportedFilePath(name);

      if (suffix == "mscz" || suffix == "mscx") {
            Score::FileError rv = score->loadMsc(name, ignoreVersionError);
            if (score && score->masterScore()->fileInfo()->path().startsWith(":/"))
                  score->setCreated(true);
            if (rv != Score::FileError::FILE_NO_ERROR)
                  return rv;
            }
      else if (suffix == "sf2" || suffix == "sf3") {
            importSoundfont(name);
            return Score::FileError::FILE_IGNORE_ERROR;
            }
      else {
            // typedef Score::FileError (*ImportFunction)(MasterScore*, const QString&);
            struct ImportDef {
                  const char* extension;
                  // ImportFunction importF;
                  Score::FileError (*importF)(MasterScore*, const QString&);
                  };
            static const ImportDef imports[] = {
                  { "xml",  &importMusicXml           },
                  { "mxl",  &importCompressedMusicXml },
                  { "mid",  &importMidi               },
                  { "midi", &importMidi               },
                  { "kar",  &importMidi               },
                  { "md",   &importMuseData           },
                  { "mgu",  &importBB                 },
                  { "sgu",  &importBB                 },
                  { "cap",  &importCapella            },
                  { "capx", &importCapXml             },
                  { "ove",  &importOve                },
                  { "scw",  &importOve                },
#ifdef OMR
                  { "pdf",  &importPdf                },
#endif
                  { "bww",  &importBww                },
                  { "gtp",  &importGTP                },
                  { "gp3",  &importGTP                },
                  { "gp4",  &importGTP                },
                  { "gp5",  &importGTP                },
                  { "gpx",  &importGTP                },
                  };

            // import
            if (!preferences.importStyleFile.isEmpty()) {
                  QFile f(preferences.importStyleFile);
                  // silently ignore style file on error
                  if (f.open(QIODevice::ReadOnly))
                        score->style()->load(&f);
                  }
            else {
                  if (score->style()->value(StyleIdx::chordsXmlFile).toBool())
                        score->style()->chordList()->read("chords.xml");
                  score->style()->chordList()->read(score->styleSt(StyleIdx::chordDescriptionFile));
                  }
            bool found = false;
            for (auto i : imports) {
                  if (i.extension == suffix) {
                        Score::FileError rv = (*i.importF)(score, name);
                        if (rv != Score::FileError::FILE_NO_ERROR)
                              return rv;
                        found = true;
                        break;
                        }
                  }
            if (!found) {
                  qDebug("unknown file suffix <%s>, name <%s>", qPrintable(suffix), qPrintable(name));
                  return Score::FileError::FILE_UNKNOWN_TYPE;
                  }
            score->setMetaTag("originalFormat", suffix);
            score->connectTies();
            score->setCreated(true); // force save as for imported files
            }

      score->rebuildMidiMapping();
      score->setSoloMute();
      for (Score* s : score->scoreList()) {
            s->setPlaylistDirty();
            s->addLayoutFlags(LayoutFlag::FIX_PITCH_VELO);
            s->setLayoutAll();
            }
      score->updateChannel();
      score->setSaved(false);
      score->update();

      if (!ignoreVersionError && !MScore::noGui)
            if (!score->sanityCheck(QString()))
                  return Score::FileError::FILE_CORRUPTED;
      return Score::FileError::FILE_NO_ERROR;
      }

//---------------------------------------------------------
//   saveAs
//    return true on success
//---------------------------------------------------------

/**
 Save the current score using a different name or type.
 Handles the GUI's file-save-as and file-save-a-copy actions.
 The saveCopy flag, if true, does not change the name of the active score nor marks it clean.
 Return true if OK and false on error.
 */

bool MuseScore::saveAs(Score* cs, bool saveCopy)
      {
      QStringList fl;
      fl.append(tr("MuseScore File") + " (*.mscz)");
      fl.append(tr("Uncompressed MuseScore File") + " (*.mscx)");     // for debugging purposes
      QString saveDialogTitle = saveCopy ? tr("MuseScore: Save a Copy") :
                                           tr("MuseScore: Save As");

      QSettings settings;
      if (mscore->lastSaveCopyDirectory.isEmpty())
            mscore->lastSaveCopyDirectory = settings.value("lastSaveCopyDirectory", preferences.myScoresPath).toString();
      if (mscore->lastSaveDirectory.isEmpty())
            mscore->lastSaveDirectory = settings.value("lastSaveDirectory", preferences.myScoresPath).toString();
      QString saveDirectory = saveCopy ? mscore->lastSaveCopyDirectory : mscore->lastSaveDirectory;

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.myScoresPath;

      QString name;
#ifdef Q_OS_WIN
      if (QSysInfo::WindowsVersion == QSysInfo::WV_XP) {
            if (!cs->isMaster())
                  name = QString("%1/%2-%3").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->fileInfo()->completeBaseName()));
            else
                  name = QString("%1/%2").arg(saveDirectory).arg(cs->fileInfo()->completeBaseName());
            }
      else
#endif
      if (!cs->isMaster())
            name = QString("%1/%2-%3.mscz").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->fileInfo()->completeBaseName()));
      else
            name = QString("%1/%2.mscz").arg(saveDirectory).arg(cs->fileInfo()->completeBaseName());

      QString filter = fl.join(";;");
      QString fn     = mscore->getSaveScoreName(saveDialogTitle, name, filter);
      if (fn.isEmpty())
            return false;

      QFileInfo fi(fn);
      if (saveCopy)
            mscore->lastSaveCopyDirectory = fi.absolutePath();
      else
            mscore->lastSaveDirectory = fi.absolutePath();

      if (fi.suffix().isEmpty()) {
            if (!MScore::noGui)
                  QMessageBox::critical(mscore, tr("MuseScore: Save As"), tr("Cannot determine file type"));
            return false;
            }
      return saveAs(cs, saveCopy, fn, fi.suffix());
      }

//---------------------------------------------------------
//   saveSelection
//    return true on success
//---------------------------------------------------------

bool MuseScore::saveSelection(Score* cs)
      {
      if (!cs->selection().isRange()) {
            if(!MScore::noGui) QMessageBox::warning(mscore, tr("MuseScore: Save Selection"), tr("Please select one or more measures"));
            return false;
            }
      QStringList fl;
      fl.append(tr("MuseScore File") + " (*.mscz)");
      QString saveDialogTitle = tr("MuseScore: Save Selection");

      QSettings settings;
      if (mscore->lastSaveDirectory.isEmpty())
            mscore->lastSaveDirectory = settings.value("lastSaveDirectory", preferences.myScoresPath).toString();
      QString saveDirectory = mscore->lastSaveDirectory;

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.myScoresPath;

      QString name   = QString("%1/%2.mscz").arg(saveDirectory).arg(cs->fileInfo()->completeBaseName());
      QString filter = fl.join(";;");
      QString fn     = mscore->getSaveScoreName(saveDialogTitle, name, filter);
      if (fn.isEmpty())
            return false;

      QFileInfo fi(fn);
      mscore->lastSaveDirectory = fi.absolutePath();

      QString ext = fi.suffix();
      if (ext.isEmpty()) {
            QMessageBox::critical(mscore, tr("MuseScore: Save Selection"), tr("Cannot determine file type"));
            return false;
            }
      bool rv = true;
      try {
            cs->saveCompressedFile(fi, true);
            }
      catch (QString s) {
            rv = false;
            QMessageBox::critical(this, tr("MuseScore: Save Selected"), s);
            }
      return rv;
      }

//---------------------------------------------------------
//   addImage
//---------------------------------------------------------

void MuseScore::addImage(Score* score, Element* e)
      {
      QString fn = QFileDialog::getOpenFileName(
         0,
         tr("MuseScore: Insert Image"),
         "",            // lastOpenPath,
         tr("All Supported Files") + " (*.svg *.jpg *.jpeg *.png);;" +
         tr("Scalable Vector Graphics") + " (*.svg);;" +
         tr("JPEG") + " (*.jpg *.jpeg);;" +
         tr("PNG") + " (*.png)"
         );
      if (fn.isEmpty())
            return;

      QFileInfo fi(fn);
      Image* s = new Image(score);
      QString suffix(fi.suffix().toLower());

      if (suffix == "svg")
            s->setImageType(ImageType::SVG);
      else if (suffix == "jpg" || suffix == "jpeg" || suffix == "png")
            s->setImageType(ImageType::RASTER);
      else
            return;
      s->load(fn);
      s->setParent(e);
      score->undoAddElement(s);
      }

#if 0
//---------------------------------------------------------
//   trim
//    returns copy of source with whitespace trimmed and margin added
//---------------------------------------------------------

static QRect trim(QImage source, int margin)
      {
      int w = source.width();
      int h = source.height();
      int x1 = w;
      int x2 = 0;
      int y1 = h;
      int y2 = 0;
      for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) {
                  QRgb c = source.pixel(x, y);
                  if (c != 0 && c != 0xffffffff) {
                        if (x < x1)
                              x1 = x;
                        if (x > x2)
                              x2 = x;
                        if (y < y1)
                              y1 = y;
                        if (y > y2)
                              y2 = y;
                        }
                  }
            }
      int x = qMax(x1 - margin, 0);
      int y = qMax(y1 - margin, 0);
      w = qMin(w, x2 + 1 + margin) - x;
      h = qMin(h, y2 + 1 + margin) - y;
      return QRect(x, y, w, h);
      }
#endif

//---------------------------------------------------------
//   savePng
//    return true on success
//---------------------------------------------------------

bool MuseScore::savePng(Score* score, const QString& name)
      {
      return savePng(score, name, false, preferences.pngTransparent, converterDpi, trimMargin, QImage::Format_ARGB32_Premultiplied);
      }

//---------------------------------------------------------
//   savePng with options
//    return true on success
//---------------------------------------------------------

bool MuseScore::savePng(Score* score, const QString& name, bool screenshot, bool transparent, double convDpi, int trimMargin, QImage::Format format)
      {
      bool rv = true;
      score->setPrinting(!screenshot);    // dont print page break symbols etc.

      QImage::Format f;
      if (format != QImage::Format_Indexed8)
          f = format;
      else
          f = QImage::Format_ARGB32_Premultiplied;

      const QList<Page*>& pl = score->pages();
      int pages = pl.size();

      int padding = QString("%1").arg(pages).size();
      bool overwrite = false;
      bool noToAll = false;
      for (int pageNumber = 0; pageNumber < pages; ++pageNumber) {
            Page* page = pl.at(pageNumber);

            QRectF r;
            if (trimMargin >= 0) {
                  QMarginsF margins(trimMargin, trimMargin, trimMargin, trimMargin);
                  r = page->tbbox() + margins;
                  }
            else
                  r = page->abbox();
            int w = lrint(r.width()  * convDpi / DPI);
            int h = lrint(r.height() * convDpi / DPI);

            QImage printer(w, h, f);
            printer.setDotsPerMeterX(lrint((convDpi * 1000) / MMPI));
            printer.setDotsPerMeterY(lrint((convDpi * 1000) / MMPI));

            printer.fill(transparent ? 0 : 0xffffffff);

            double mag = convDpi / DPI;
            QPainter p(&printer);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            p.scale(mag, mag);
            if (trimMargin >= 0)
                  p.translate(-r.topLeft());

            QList<Element*> pel = page->elements();
            qStableSort(pel.begin(), pel.end(), elementLessThan);
            paintElements(p, pel);

            if (format == QImage::Format_Indexed8) {
                  //convert to grayscale & respect alpha
                  QVector<QRgb> colorTable;
                  colorTable.push_back(QColor(0, 0, 0, 0).rgba());
                  if (!transparent) {
                        for (int i = 1; i < 256; i++)
                              colorTable.push_back(QColor(i, i, i).rgb());
                        }
                  else {
                        for (int i = 1; i < 256; i++)
                              colorTable.push_back(QColor(0, 0, 0, i).rgba());
                        }
                  printer = printer.convertToFormat(QImage::Format_Indexed8, colorTable);
                  }

            QString fileName(name);
            if (fileName.endsWith(".png"))
                  fileName = fileName.left(fileName.size() - 4);
            fileName += QString("-%1.png").arg(pageNumber+1, padding, 10, QLatin1Char('0'));
            if (!converterMode) {
                  QFileInfo fip(fileName);
                  if(fip.exists() && !overwrite) {
                        if(noToAll)
                              continue;
                        QMessageBox msgBox( QMessageBox::Question, tr("Confirm Replace"),
                              tr("\"%1\" already exists.\nDo you want to replace it?\n").arg(QDir::toNativeSeparators(fileName)),
                              QMessageBox::Yes |  QMessageBox::YesToAll | QMessageBox::No |  QMessageBox::NoToAll);
                        msgBox.setButtonText(QMessageBox::Yes, tr("Replace"));
                        msgBox.setButtonText(QMessageBox::No, tr("Skip"));
                        msgBox.setButtonText(QMessageBox::YesToAll, tr("Replace All"));
                        msgBox.setButtonText(QMessageBox::NoToAll, tr("Skip All"));
                        int sb = msgBox.exec();
                        if(sb == QMessageBox::YesToAll) {
                              overwrite = true;
                              }
                        else if (sb == QMessageBox::NoToAll) {
                              noToAll = true;
                              continue;
                              }
                        else if (sb == QMessageBox::No)
                              continue;
                        }
                  }
            rv = printer.save(fileName, "png");
            if (!rv)
                  break;
            }
      score->setPrinting(false);
      return rv;
      }

//---------------------------------------------------------
//   WallpaperPreview
//---------------------------------------------------------

WallpaperPreview::WallpaperPreview(QWidget* parent)
   : QFrame(parent)
      {
      _pixmap = 0;
      }

//---------------------------------------------------------
//   paintEvent
//---------------------------------------------------------

void WallpaperPreview::paintEvent(QPaintEvent* ev)
      {
      QPainter p(this);
      int fw = frameWidth();
      QRect r(frameRect().adjusted(fw, fw, -2*fw, -2*fw));
      if (_pixmap)
            p.drawTiledPixmap(r, *_pixmap);
      QFrame::paintEvent(ev);
      }

//---------------------------------------------------------
//   setImage
//---------------------------------------------------------

void WallpaperPreview::setImage(const QString& path)
      {
      qDebug("setImage <%s>", qPrintable(path));
      delete _pixmap;
      _pixmap = new QPixmap(path);
      update();
      }

//---------------------------------------------------------
//   getWallpaper
//---------------------------------------------------------

QString MuseScore::getWallpaper(const QString& caption)
      {
      QString filter = tr("Images") + " (*.jpg *.jpeg *.png);;" + tr("All") + " (*)";
      QString d = mscoreGlobalShare + "/wallpaper";

      if (preferences.nativeDialogs) {
            QString s = QFileDialog::getOpenFileName(
               this,                            // parent
               caption,
               d,
               filter
               );
            return s;
            }

      if (loadBackgroundDialog == 0) {
            loadBackgroundDialog = new QFileDialog(this);
            loadBackgroundDialog->setFileMode(QFileDialog::ExistingFile);
            loadBackgroundDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            loadBackgroundDialog->setWindowTitle(caption);
            loadBackgroundDialog->setNameFilter(filter);
            loadBackgroundDialog->setDirectory(d);

            QSettings settings;
            loadBackgroundDialog->restoreState(settings.value("loadBackgroundDialog").toByteArray());
            loadBackgroundDialog->setAcceptMode(QFileDialog::AcceptOpen);

            QSplitter* splitter = loadBackgroundDialog->findChild<QSplitter*>("splitter");
            if (splitter) {
                  WallpaperPreview* preview = new WallpaperPreview;
                  splitter->addWidget(preview);
                  connect(loadBackgroundDialog, SIGNAL(currentChanged(const QString&)),
                     preview, SLOT(setImage(const QString&)));
                  }
            }

      //
      // setup side bar urls
      //
      QList<QUrl> urls;
      QString home = QDir::homePath();
      urls.append(QUrl::fromLocalFile(d));
      urls.append(QUrl::fromLocalFile(home));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));
      loadBackgroundDialog->setSidebarUrls(urls);

      if (loadBackgroundDialog->exec()) {
            QStringList result = loadBackgroundDialog->selectedFiles();
            return result.front();
            }
      return QString();
      }

///////////////////////////////////////////////////////////////////////////////
// SVG function:
//   saveSvg()         the one, the original, fully integrated into MuseScore (not quite fully yet...)
// Shared SVG and SMAWS functions:
//   svgInit()         initializes variables prior exporting a score
//   paintStaffLines() paints SVG staff lines more efficiently
// Pure SMAWS:
//   saveSMAWS_Music() writes linked SVG and VTT files for animation
//   saveVTT()         a helper function exclusively for saveSMAWS_Music()
//   getCueID()        another little helper (Cue IDs link SVG to VTT)
//   getAnnCueID()     gets Annotation Cue ID (Harmony and RehearsalMark)
//   getScrollCueID()  gets Cue ID for scrolling cues and RehearsalMarks
///////////////////////////////////////////////////////////////////////////////

// 3 Cue ID generators: getCueID(), getAnnCueID(), getScrollCueID()
// Cue IDs -> Start & End | Ticks & Time:
// VTT needs start & end times, and I need ticks to calculate time.
// cue_id is what links the SVG elements to the VTT cues.
// It is a unique id because it is in this format:
//     "startTick_endTick"
// and startTick + endTick (duration) is unique to each cue, which links
// to one or more SVG Notes, BarLines, etc., across staves/voices.

// startMSecsFromTick()
// Returns the Start Milliseconds for a MIDI tick
static int startMSecsFromTick(Score* score, int tick)
{
    return qRound(score->tempomap()->tick2time(qMax(tick, 0)) * 1000);
}

// startMSecsFromCueID()
// Returns the Start Milliseconds for a Cue ID
//!!!OBSOLETE!!!
//!!!static int startMSecsFromCueID(Score* score, QString& cue_id)
//!!!{
//!!!    if (!cue_id.isEmpty())
//!!!        return startMSecsFromTick(score, cue_id.left(CUE_ID_FIELD_WIDTH).toInt());
//!!!    else
//!!!        return 0;
//!!!}

// getCueID()
// Creates a cue ID string from a start/end tick value pair
static QString getCueID(int startTick, int endTick = -1)
{
    // For cue_id formatting: 1234567_0000007
    const int   base       = 10;
    const QChar fillChar   = SVG_ZERO;

    // Missing endTick means zero duration cue, only one value required
    if (endTick < 0)
        endTick = startTick;

    return QString("%1_%2").arg(startTick, CUE_ID_FIELD_WIDTH, base, fillChar)
                             .arg(endTick, CUE_ID_FIELD_WIDTH, base, fillChar);
}

// getAnnCueID()
// Gets the cue ID for an annotation, such as rehearsal mark or chord symbol,
// where the cue duration lasts until the next element of the same type.
static QString getAnnCueID(Score* score, const Element* e, EType eType)
{
    Segment* segStart = static_cast<Segment*>(e->parent());
    int     startTick = segStart->tick();

    for (Segment* seg = segStart->next1MM(Segment::Type::ChordRest);
                  seg; seg = seg->next1MM(Segment::Type::ChordRest)) {
        for (Element* eAnn : seg->annotations()) {
            if (eAnn->type() == eType)
                return getCueID(startTick, seg->tick());
        }
    }

    // If there's no "next" annotation, this is the last one in the score
    return getCueID(startTick, score->lastSegment()->tick());
}
// getScrollCueID()
// Gets the cue ID for zero-duration (scrolling) cues + rehearsal marks:
//  - ruler: BarLine or RehearsalMark
//  - frozen pane: Clef, KeySig, TimeSig, Tempo, InstrumentName, InstrumentChange
static QString getScrollCueID(Score* score, const Element* e)
{
    QString cue_id = "";
    EType   eType  = e->type();

    // Always exclude invisible elements, except TEMPO_TEXT + INSTRUMENT_CHANGE
    if (!e->visible() && (eType != EType::TEMPO_TEXT
                       || eType != EType::INSTRUMENT_CHANGE))
        return cue_id;

    Element* p = e->parent();
    switch (eType) {
    case EType::BAR_LINE       :
        // There are N + 1 BarLines per System
        //     where N = number-of-measures-in-this-system
        //    (# of barlines/system is variable, not fixed)
        // Each Measure has only 1 BarLine, at its right edge (end-of-measure)
        // Each System's first BarLine is a System BarLine  (parent == System)
        switch (p->type()) {
        case EType::SYSTEM  :
            // System BarLines only used for scrolling = zero duration
            // System::firstMeasure() has the tick we need
            cue_id = getCueID(static_cast<System*>(p)->firstMeasure()->tick());
            break;
        case EType::SEGMENT :
            // Measure BarLines are also zero duration, used for scrolling
            // and the rulers' playback position cursors.
            // RehearsalMarks only animate in the ruler, not in the score,
            // and they have full-duration cues, marker-to-marker.
            cue_id = getCueID(static_cast<Measure*>(p->parent())->tick());
            break;
        default:
            break; // Should never happen
        }
        break;
    case EType::REHEARSAL_MARK :
        cue_id = getAnnCueID(score, e, eType);
        break;
    case EType::TEMPO_TEXT :
    case EType::CLEF       :
    case EType::KEYSIG     :
    case EType::TIMESIG    :
        cue_id = getCueID(static_cast<Segment*>(p)->tick());
        break;
    case EType::INSTRUMENT_NAME :
        cue_id = CUE_ID_ZERO;
        break;
    case EType::INSTRUMENT_CHANGE :
        cue_id = getCueID(static_cast<const InstrumentChange*>(e)->segment()->tick());
        break;
    default: // Non-scrolling element types return an empty cue_id
        break;
    }

    return cue_id;
}

// Returns a full data-cue="cue1,cue2,..." string of comma-separated cue ids
static QString getGrayOutCues(Score* score, int idxStaff, QStringList* pVTT)
{
    ///!!!For now these cues are not used in sheet music .svg files. The only
    ///!!!gray-out cues are in the Mix Tree .vtt file.
    return(QString(""));
    ///!!!

    int  startTick;
    bool hasCues    = false;
    bool isPrevRest = false;

    QString     cue_id;
    QString     cues;
    QTextStream qts(&cues);

    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasureMM())
    {
        // Empty measure = all rests
        if (m->isMeasureRest(idxStaff)) {
            if (!isPrevRest) {         // Start of gray-out cue
                isPrevRest = true;
                startTick  = m->tick();
            }

            if (!m->nextMeasureMM()) { // Final measure is empty
                if (hasCues)
                    qts << SVG_COMMA;
                else  {
                    qts << SVG_CUE;
                    hasCues = true;
                }

                cue_id = getCueID(startTick, m->tick() + m->ticks());
                qts << cue_id;
                pVTT->append(cue_id);
            }
        }
        else {
            if (isPrevRest) {          // Complete any pending gray-out cue
                if (hasCues)
                    qts << SVG_COMMA;
                else  {
                    qts << SVG_CUE;
                    hasCues = true;
                }

                cue_id = getCueID(startTick, m->tick());
                qts << cue_id;
                pVTT->append(cue_id);
            }

            isPrevRest = false;
        }
    }

    if (hasCues)
        qts << SVG_QUOTE;

    return(cues);
}

// Does a score have chord changes specified? (EType::HARMONY = chord "symbol")
// Used by saveSMAWS_Tables()
static Harmony* getHarmony(Segment* seg) {
    Harmony* pHarm = 0;
    for (Element* eAnn : seg->annotations()) {
        if (eAnn->type() == EType::HARMONY) {
            pHarm = static_cast<Harmony*>(eAnn); // found one, we're done
            break;
        }
    }

    return pHarm;
}

// paintStaffLines() - consolidates code in saveSVG() and saveSMAWS_Music()
//                     for MuseScore master, no harm, no gain, 100% neutral
static void paintStaffLines(Score*        score,
                            QPainter*     p,
                            SvgGenerator* printer,
                            Page*         page,
                            QVector<int>* pVisibleStaves =  0,
                            int           nVisible       =  0,
                            int           idxStaff       = -1, // element.staffIdx()
                            bool          isMulti        = false,
                            QStringList*  pINames        =  0,
                            QList<qreal>* pStaffTops     =  0,
                            QStringList*  pVTT         =  0)
{
    const qreal cursorOverlap = Ms::SPATIUM20 / 2; // half staff-space overlap, top + bottom

    bool  isFirstSystem = true;
    qreal cursorTop;
    qreal cursorBot;
    qreal vSpacerUp = 0;

    if (isMulti && idxStaff > -1 && pINames != 0  && pVisibleStaves != 0) {
        // isMulti requires a <g></g> wrapper around each staff's elements
        QString qs = score->systems().first()->staff(idxStaff)->instrumentNames.first()->xmlText(); ///!!!this line of code crashes for piano-style dual-staff (linked staves?)!!!
        pINames->append(qs.replace(SVG_SPACE, SVG_DASH));

        const bool isTab     = score->staff(idxStaff)->isTabStaff();
        const int gridHeight = 30;
        const int tabHeight  = 53;
        const int stdHeight  = 45;                //!!! see below
        const int idxVisible = (*pVisibleStaves)[idxStaff];
        const System* s      = page->systems().value(0);
        qreal top = s->staff(idxStaff)->y();
        qreal bot = -1;
        if (s->firstMeasure()->mstaff(idxStaff)->_vspacerUp != 0) {
            // This measure claims the extra space between it and the staff above it
            // Get the previous visible staff - top staff can't have a vertical spacer up
            for (int i = idxStaff - 1; i >= 0; i--) {
                if ((*pVisibleStaves)[i] > 0) {
                    vSpacerUp = top - (s->staff(i)->y() + (score->staff(i)->isTabStaff() ? tabHeight : stdHeight));
                    break;
                }
            }
        }

        if (idxVisible >= 0 && idxVisible < nVisible - 1) {
            // Get the next visible staff (below)
            for (int i = idxStaff + 1; i < pVisibleStaves->size(); i++) {
                if ((*pVisibleStaves)[i] > 0) {
                    if (s->firstMeasure()->mstaff(i)->_vspacerUp != 0)
                        // Next staff (below) claims the extra space below this staff
                        bot = top + (isTab ? tabHeight : stdHeight); //!!! I assume that this staff's height is the standard 45pt/px or 53 for tablature
                    else
                        bot = s->staff(i)->y(); // top of next visible staff
                    break;
                }
            }
        }
        top -= vSpacerUp;

        if (bot < 0)
            bot = page->height() - pStaffTops->value(0) - page->bm();

        // Standard notation, tablature, or grid? I need to know by staff
        const QString shortName = score->staff(idxStaff)->part()->shortName(0);
        const bool    isGrid    = (shortName == STAFF_GRID);
        const qreal   height    = (isGrid ? gridHeight : bot - top);
        const QString className = (isGrid ? CLASS_GRID
                                          : (isTab ? CLASS_TABS
                                                   : CLASS_NOTES));
        printer->beginMultiGroup(pINames, shortName, className, height, top,
                                 getGrayOutCues(score, idxStaff, pVTT));     //!!! these gray-out cues are not currently used
        printer->setCueID("");
    }

    bool isVertical = printer->isScrollVertical();
    for (System* s : page->systems()) {
        for (int i = 0, n = s->staves()->size(); i < n; i++) {
            if (idxStaff > -1)
                i = idxStaff; // Only one staff's lines being drawn

            // SMAWS scores need the top y-coord and height of the first system
            // for the highlight cursor. This is the best place to calculate it,
            // especially for vertical scores. It assumes all systems are the
            // same height. Systems and staves are in top-to-bottom order here.
            if (pVisibleStaves != 0) {
                printer->setStaffIndex(pVisibleStaves->value(i));
                if (isFirstSystem) {
                    StaffLines* sl = s->firstMeasure()->staffLines(i);
                    qreal staffTop = sl->bbox().top() + sl->pagePos().y();
                    int j;
                    // Get the first visible staff index (in the first system)
                    for (j = 0; j < pVisibleStaves->size(); j++) {
                        if (pVisibleStaves->value(j) >= 0)
                            break;
                    }
                    if (i == j) { // First visible staff in the first system
                        // Set the cursor's y-coord
                        cursorTop = staffTop - cursorOverlap;
                        printer->setCursorTop(cursorTop);

                        // Get the last visible staff index in the first system
                        for (j = pVisibleStaves->size() - 1; j >= 0; j--) {
                            if (pVisibleStaves->value(j) >= 0)
                                break;
                        }
                        // Set the cursor's height
                        sl        = s->firstMeasure()->staffLines(j);
                        cursorBot = sl->bbox().top()
                                  + sl->pagePos().y()
                                  + score->staff(i)->height() // bbox().bottom() includes margins
                                  + cursorOverlap;
                        printer->setCursorHeight(cursorBot - cursorTop);
                    }
                    if (isMulti && pStaffTops != 0) {
                        // Offset between this staff and the first visible staff
                        staffTop -= vSpacerUp;
                        pStaffTops->append(staffTop);
                        printer->setYOffset((*pStaffTops)[0] - staffTop);
                    }
                }
            }

            // staff with invisible staff lines = nothing to draw here
            if (score->staff(i)->invisible())
                continue;

            // The goal here is to draw SVG staff lines more efficiently.
            // MuseScore draws staff lines by measure, but for SVG they can
            // generally be drawn once for each system. This makes a big
            // difference for scores that scroll horizontally on a single
            // page. But there are exceptions to this rule:
            //
            //   ~ One (or more) invisible measure(s) in a system/staff ~
            //   ~ One (or more) elements of type HBOX or VBOX          ~
            //
            // In these cases the SVG staff lines for the system/staff
            // are drawn by measure.
            //
            bool byMeasure = false;
            for (MeasureBase* mb = s->firstMeasure(); mb != 0; mb = s->nextMeasure(mb)) {
                  if (mb->type() == Element::Type::HBOX
                   || mb->type() == Element::Type::VBOX
                   || !static_cast<Measure*>(mb)->visible(i)) {
                        byMeasure = true;
                        break;
                  }
            }
            if (byMeasure) { // Draw visible staff lines by measure
                  for (MeasureBase* mb = s->firstMeasure(); mb != 0; mb = s->nextMeasure(mb)) {
                        if (mb->type() != Element::Type::HBOX
                         && mb->type() != Element::Type::VBOX
                         && static_cast<Measure*>(mb)->visible(i)) {
                              StaffLines* sl = static_cast<Measure*>(mb)->staffLines(i);
                              printer->setElement(sl);
                              paintElement(*p, sl);
                        }
                  }
            }
            else { // Draw staff lines once per system
                QString     cue_id;
                StaffLines* firstSL = s->firstMeasure()->staffLines(i)->clone();
                StaffLines*  lastSL =  s->lastMeasure()->staffLines(i);
                firstSL->bbox().setRight(lastSL->bbox().right()
                                      +  lastSL->pagePos().x()
                                      - firstSL->pagePos().x());
                if (isVertical && i == 0)
                    cue_id = getCueID(s->firstMeasure()->tick());
                else
                    cue_id = "";

                printer->setCueID(cue_id);
                printer->setElement(firstSL);
                paintElement(*p, firstSL);
            }

            if (idxStaff != -1) // No need to break from outer loop,
                break;          // because score has only one system.

        } // for each Staff
        isFirstSystem = false;

    } //for each System

}

// svgInit() - consolidates shared code in saveSVG and saveSMAWS.
//             for MuseScore master, no harm, no gain, 100% neutral
static bool svgInit(Score*        score,
              const QString&      saveName,
                    SvgGenerator* printer,
                    QPainter*     p,
                    bool          bSMAWS = false)
{
    printer->setFileName(saveName);
    if (!p->begin(printer))
        return false;

    printer->setTitle(score->title());
    score->setPrinting(true);
    MScore::pdfPrinting = true;
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    QRectF r;
    if (trimMargin >= 0 && score->npages() == 1) {
        QMarginsF margins(trimMargin, trimMargin, trimMargin, trimMargin);
        r = score->pages().first()->tbbox() + margins;
    }
    else {
        const PageFormat* pf = score->pageFormat();
        r = QRectF(0, 0, pf->width() * score->pages().size(), pf->height());
    }
    qreal w = r.width();
    qreal h = r.height();
    if (trimMargin >= 0 && score->npages() == 1)
          p->translate(-r.topLeft());

    // The relationship between the viewBox dimensions and the width/height
    // values, combined with the preserveAspectRatio value, determine default
    // scaling inside the SVG file. (preserveAspectRatio="xMinYMin slice")
    printer->setViewBox(QRectF(0, 0, w, h));
    if (bSMAWS)
        // SMAWS: No scaling inside the SVG (scale factor = 1)
        printer->setSize(QSize(w, h));
    else {
        // MuseScore: SVG scale factor = screen scale factor
        qreal scaleFactor;
        scaleFactor = 1; //!!!TODO: get scale factor
        printer->setSize(QSize(w * scaleFactor, h * scaleFactor));
    }

    return true;
}

// MuseScore::saveSvg() - This version is compatible with MuseScore master if
//                        svgInit() + paintStaffLines() are integrated as well.
bool MuseScore::saveSvg(Score* score, const QString& saveName)
{
    // Initialize
    SvgGenerator printer;
    QPainter p;
    if (!svgInit(score, saveName, &printer, &p))
        return false;

    // Print/paint/draw/whatever-you-want-to-call-it
    foreach (Page* page, score->pages()) {
        // 1st pass: StaffLines
        paintStaffLines(score, &p, &printer, page);

        // 2nd pass: the rest of the elements
        QList<Element*> pel = page->elements();
        qStableSort(pel.begin(), pel.end(), elementLessThan);

        Element::Type eType;
            for (const Element* e : pel) {
            // Always exclude invisible elements
            if (!e->visible())
                    continue;

            eType = e->type();
            switch (eType) { // In future sub-type code, this switch() grows, and eType gets used
            case EType::STAFF_LINES : // Handled in the 1st pass above
                continue; // Exclude from 2nd pass
                break;
            default:
                break;
            } // switch(eType)

            // Set the Element pointer inside SvgGenerator/SvgPaintEngine
            printer.setElement(e);

            // Paint it
            paintElement(p, e);
        }
        // Future code will create separate files for each page,
        // so this horizontal offset page-simulation will disappear.
        p.translate(QPointF(score->pageFormat()->width(), 0.0));
    }

      // Clean up and return
      score->setPrinting(false);
      MScore::pdfPrinting = false;
      p.end(); // Writes MuseScore SVG file to disk, finally
      return true;
}
///////////////////////////////////////////////////////////////////////////////
// SMAWS functions - 100% outside of MuseScore master (at least for now...)
///////////////////////////////////////////////////////////////////////////////

// Static helper functions
// ticks2VTTmsecs()  - converts from ticks to hh:mm:ss:zzz string for VTT cue
// getVTTCueTwo()    - returns the first 2 lines of a SMAWS VTT cue: id + time
// smawsDesc()       - returns a string to use in the <desc> of SVG file
// paintStaffSMAWS() - paints the entire score for one staff
// elementLessThanByStaff() - sort-by-staff function for std::stable_sort()
//
static int ticks2msecs(int ticks, const TempoMap* tempos) {
    return qRound(tempos->tick2time(ticks) * 1000);
}

static QString ticks2VTTmsecs(int ticks, const TempoMap* tempos) {
    return QTime::fromMSecsSinceStartOfDay(ticks2msecs(ticks, tempos)).toString("hh:mm:ss.zzz");
}

// Start-time-only cues. Currently only Fretboards use them.
// Return the cue's first two lines: 123 (not fixed width)
//                                   00:00:00.000 --> 00:00:00.001
static QString getVTTStartCue(int tick, const TempoMap* tempos) {
    int msecs = ticks2msecs(tick, tempos);
    return QString(VTT_CUE_3_ARGS).arg(QString::number(tick))
                                  .arg(QTime::fromMSecsSinceStartOfDay(msecs).toString("hh:mm:ss.zzz"))
                                  .arg(QTime::fromMSecsSinceStartOfDay(msecs + 1).toString("hh:mm:ss.zzz"));
}

// getVTTCueTwo()
// Gets the first two lines of a VTT cue. The minimum valid cue requires only
// an additional newline. Some cues have cue text in addition to that.
// WebVTT requires that end times be greater than start times by at least 1ms.
// I have scrolling cues that are zero duration, but in VTT they must last 1ms.
static QString getVTTCueTwo(const QString& cue_id, const TempoMap* tempos)
{
    // Split the cue_id into start and end ticks
    const int startTick = cue_id.left( CUE_ID_FIELD_WIDTH).toInt();
    const int   endTick = cue_id.right(CUE_ID_FIELD_WIDTH).toInt();
    const int   endTime = ticks2msecs(endTick, tempos) + (startTick == endTick ? 1 : 0);


    // Return the cue's first two lines: 0000000_1234567
    //                                   00:00:00.000 --> 12:34:56.789
    return QString(VTT_CUE_3_ARGS).arg(cue_id)
                                  .arg(ticks2VTTmsecs(startTick, tempos))
                                  .arg(QTime::fromMSecsSinceStartOfDay(endTime).toString("hh:mm:ss.zzz"));
}

// This gets used a couple/few times
static QString smawsDesc(Score* score) {
    return QString(SMAWS_DESC_STUB).arg(score->metaTag("copyright"))
                                   .arg(VERSION)
                                   .arg(SMAWS_VERSION);
}

// Paints the animated elements specified in the SVGMaps
static void paintStaffSMAWS(Score*        score,
                            QPainter*     p,
                            SvgGenerator* printer,
                            SVGMap*       mapFrozen,
                            SVGMap*       mapSVG,
                            SVGMap*       mapLyrics,
                            QVector<int>* pVisibleStaves =  0,
                            QList<qreal>* pStaffTops     =  0,
                            int           idxStaff = -1,
                            bool          isMulti  = false,
                            int           lyricsHeight = -1)
{
    QString cue_id;

    // 2nd pass: Animated elements
    // Animated elements are sorted in playback order by their QMaps.
    // mapFrozen goes first, if it has any contents
    if (mapFrozen->size() > 0) {
        // Iterate by key, then by value in reverse order. This recreates the
        // MuseScore draw order within the key/cue_id. This is required for
        // the frozen pane to generate properly.
        QStringList keys = mapFrozen->uniqueKeys();
        for (QStringList::iterator c = keys.begin(); c != keys.end(); ++c) {
            printer->setCueID(*c);
            QList<const Element*> values = mapFrozen->values(*c);
            for (int i = values.size() - 1; i > -1; i--) {
                const Element* e = values[i];
                if (!isMulti)
                    printer->setStaffIndex(pVisibleStaves->value(e->staffIdx()));
                printer->setElement(e);
                paintElement(*p, e);
            }
            // Complete one frozen pane def: if (idxStaff == -1) by cue_id
            //                               else      by staff, by cue_id
            printer->freezeIt(pVisibleStaves->value(idxStaff));
        }
    }

    // mapSVG (in reverse draw order, not a problem for these element types)
    SVGMap::iterator i;
    for (i = mapSVG->begin(); i != mapSVG->end(); ++i) {
        cue_id = i.key();
        printer->setCueID(cue_id);
///!!!OBSOLETE!!!        printer->setStartMSecs(startMSecsFromCueID(score, cue_id));
        printer->setElement(i.value());
        paintElement(*p, i.value());
    }

    if (isMulti) {
        // Close any pending Multi-Select Staves group element
        printer->endMultiGroup();

        // Lyrics are a separate <g> element pseudo staff
        if (mapLyrics->size() > 0) {
            printer->beginMultiGroup(0,
                                     score->staff(idxStaff)->part()->shortName(0),
                                     "lyrics",
                                     lyricsHeight,
                                     pStaffTops->last() - pStaffTops->first(),
                                     QString());

            for (i = mapLyrics->begin(); i != mapLyrics->end(); ++i) {
                cue_id = i.key();
                printer->setCueID(cue_id);
///!!!OBSOLETE!!!                printer->setStartMSecs(startMSecsFromCueID(score, cue_id));
                printer->setElement(i.value());
                paintElement(*p, i.value());
            }

            printer->endMultiGroup();
        }
    }
}

// Helps sort elements on a page by element type, by staff
static bool elementLessThanByStaff(const Element* const e1, const Element* const e2)
{
    return e1->staffIdx() <= e2->staffIdx();
}

// Formats ints in fixed width for SVT attribute value
static QString formatInt(const QString& attr,
                         const int      i,
                         const int      maxDigits,
                         const bool     withQuotes)
{
    QString qsInt = QString::number(i);
    QString qs;
    QTextStream qts(&qs);
    qts << attr;
    qts.setFieldAlignment(QTextStream::AlignRight);
    qts.setFieldWidth(maxDigits + (withQuotes ? 2: 0));
    if (withQuotes)
        qts << QString("%1%2%3").arg(SVG_QUOTE).arg(qsInt).arg(SVG_QUOTE);
    else
        qts << QString("%1").arg(qsInt);

    return qs;
}

// Formats reals in fixed width for SVT attribute value
// Exact duplicate of SvgPaintEngine::fixedFormat()!!!
static QString formatReal(const QString& attr,
                          const qreal    n,
                          const int      maxDigits,
                          const bool     withQuotes)
{
    QString qsReal = QString::number(n, 'f', SVG_PRECISION);
    QString qs;
    QTextStream qts(&qs);
    qts << attr;
    qts.setFieldAlignment(QTextStream::AlignRight);
    qts.setFieldWidth(maxDigits + SVG_PRECISION + (withQuotes ? 2 : 0) + 1); // 1 is for decimal point as char
    if (withQuotes)
        qts << QString("%1%2%3").arg(SVG_QUOTE).arg(qsReal).arg(SVG_QUOTE);
    else
        qts << QString("%1").arg(qsReal);

    return qs;
}

///////////////////////////////////////////////////////////////////////////////
// 4 SMAWS file generators: saveVTT()          saveSMAWS_Rulers()
//                          saveSMAWS_Music()  saveSMAWS_Tables()
///////////////////////////////////////////////////////////////////////////////
// saveVTT()
// Private, static function called by saveSMAWS_Lyrics() and saveSMAWS_Frets()
// Generates the WebVTT file (.vtt) for start-time-only cues.
static bool saveStartVTT(Score* score, const QString& fileRoot, IntSet* setVTT)
{
    // Open a stream into the file
    QFile fileVTT;
    fileVTT.setFileName(QString("%1%2").arg(fileRoot).arg(EXT_VTT));
    fileVTT.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    QTextStream streamVTT(&fileVTT);

    // Stream the header
    streamVTT << VTT_START_ONLY;

    // setVTT is a real set, sorted and everything
    const TempoMap* tempos = score->tempomap();
    for (IntSet::iterator i = setVTT->begin(); i != setVTT->end(); ++i)
        streamVTT << getVTTStartCue(*i, tempos) << endl;

    // Write and close the VTT file
    streamVTT.flush();
    fileVTT.close();
    return true;
}

// saveVTT()
// Private, static function called by saveSMAWS_Music() (...and saveSMAWS_Rulers()?)
// Generates the WebVTT file (.vtt) using the setVTT arg as the data source.
static bool saveVTT(Score* score, const QString& fileRoot, QStringList& setVTT)
{
    // Open a stream into the file
    QFile fileVTT;
    fileVTT.setFileName(QString("%1%2").arg(fileRoot).arg(EXT_VTT));
    fileVTT.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    QTextStream streamVTT(&fileVTT);

    // Stream the header
    streamVTT << VTT_HEADER;

    // Change setVTT into a sorted set of unique cue IDs, then iterate over it
    setVTT.removeDuplicates();
    setVTT.sort();
    const TempoMap* tempos = score->tempomap();
    for (int i = 0, n = setVTT.size(); i < n; i++) {
        // Stream the cue: cue_id
        //                 startTime --> endTime
        //                [this line intentionally left blank, per WebVTT spec]
        streamVTT << getVTTCueTwo(setVTT[i], tempos) << endl;
    }
    // Write and close the VTT file
    streamVTT.flush();
    fileVTT.close();
    return true;
}

// MuseScore::autoSMAWS() - exports all (open) files related to this score
// In the future, confirmation dialogs might be a good idea in a few places,
// instead of fully automating all the rules, let the user choose.
bool MuseScore::autoSMAWS(Score* score, QFileInfo* qfi, bool isAll)
{
    QString type;

    const QString workNo = score->metaTag(tagWorkNo);

    if (isAll) {
        ;
    }
    else {
        // Export everything (that's open, until multi-tab support)
        for (Score* s : mscore->scores()) {
            // Ignore open files unrelated to this score
            if (workNo == s->metaTag(tagWorkNo)) {
                // tagMoveNo acts as a SMAWS file type indicator
                type = s->metaTag(tagMoveNo);
                if (type == SMAWS_TREE) {
                    saveSMAWS_Tree(  s, qfi);
                    saveSMAWS_Rulers(s, qfi);
                }
                else if (type == SMAWS_LYRICS)
                    saveSMAWS_Lyrics(s, qfi);
                else if (type == SMAWS_SCORE)
                    saveSMAWS_Music( s, qfi, true, true); // auto-exports frets
                else if (type == SMAWS_PART)
                    saveSMAWS_Music( s, qfi, false, false);
                else if (type == SMAWS_GRID)
                    saveSMAWS_Tables(s, qfi, false, false);
            }
        }
    }

    return true;
}

// MuseScore::saveSMAWS_Music() - one SVG file: Cue IDs in data-cue attribute
//                                one VTT file: linked to the Score SVG file
//                                and all of its parts' SVG files. Parts use
//                                the score's VTT file in JavaScript.
//
// isMulti == SMAWS_SCORE != SMAWS_PART
// isMulti argument outputs each staff as a separate group element in the defs
// section of the SVG file. The body of the file contains a <use> element for
// each visible staff. isMulti assumes horizontal scrolling. An extra "system"
// staff is added for chord symbols, markers, system text, measure numbers.
//
// ¡¡¡WARNING!!!
// With the page settings option for points/pixels, rounding is not an issue.
// But you must choose that option, otherwise rounding errors persist due
// to the default templates being in mm (or inches, either way).
//
bool MuseScore::saveSMAWS_Music(Score* score, QFileInfo* qfi, bool isMulti, bool isAuto)
{
    if (score->metaTag(tagWorkNo).isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Music"), tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }

    // qfi is a VTT file, this needs an SVG file
    const QString fnRoot = QString("%1/%2%3%4").arg(qfi->path())
                                               .arg(score->metaTag(tagWorkNo))
                                               .arg(SMAWS_)
                                               .arg(isMulti ? SMAWS_SCORE
                                                              : score->staff(0)->part()->longName(0));
    // Initialize MuseScore SVG Export variables
    SvgGenerator printer;
    QPainter p;
    if (!svgInit(score, QString("%1%2").arg(fnRoot).arg(EXT_SVG), &printer, &p, true))
        return false;

    // Custom SMAWS header, including proper reference to MuseScore
    printer.setDescription(smawsDesc(score));

    // The link between an SVG elements and a VTT cue. See getCueID().
    QString cue_id;

    // QSet is unordered, QStringList::removeDuplicates() creates a unique set.
    // setVTT     - a chronologically sorted set of unique cue_ids
    QStringList setVTT;

    // mapSVG      - a real map: key = cue_id; value = list of elements.
    // mapFrozen   - ditto, value = SCORE frozen pane elements
    // mapSysStaff - ditto, value = SCORE "system" staff: tempos, chords, markers (should be measure numbers!!!)
    // mapLyrics   - ditto, value = SCORE lyrics as a separate group
    SVGMap mapSVG;
    SVGMap mapFrozen;
    SVGMap mapSysStaff;
    SVGMap mapLyrics;

    // Animated elements in a multi-page file? It's unnecessary IMO.
    // + saveSvg() handles pages in SVG with a simple horizontal offset.
    // + SVG doesn't even have support for pages, per se (yet?).
    // So this code only exports the score's first page. At least for now...
    // also note: MuseScore plans to export each page as a separate file.
    Page* page = score->pages()[0];

    // General SMAWS
    printer.setSMAWS();
    printer.setCueID("");

    // isScrollVertical determines the data-scroll attribute value:
    //   true: data-scroll="y"   false: data-scroll="x"
    // Using PageFormat::twosided() is a hack, but it causes no conflicts.
    // MuseScore's landscape vs. portrait is purely based on page dimensions
    // and doesn't exist outside of the pagesettings.ui window.
    const bool isScrollVertical = score->pageFormat()->twosided();
    printer.setScrollVertical(isScrollVertical);

    // visibleStaves - Frozen Panes and Multi-Select Staves deal with visible
    // staves only. This vector is the same size as score->nstaves(). If a
    // staff is invisible its value in the vector is -1, else it contains the
    // visible-staff index for that staff.
    QVector<int> visibleStaves;
    visibleStaves.resize(score->nstaves());
    int nVisible = 0;

    // nonStdStaves - Tablature and percussion staves require special treatment
    // in the frozen pane. They don't have keysigs, and the clef never changes,
    // but the timesig needs to be properly aligned with the other staves. The
    // slashes-only "grid" staff behaves this same way - it should be created
    // as a percussion staff, that will group it into this vector and not make
    // it a non-standard height like tablature staves.
    QVector<int> nonStdStaves;
    bool hasTabs = false; // for auto-export of fretboards

    // Lyrics staves. Managing preset staff height for the lyrics pseudo-staves
    // is dicey. It only affects the display of lyrics w/o notes. This is so
    // that the last (lowest) lyrics staff has an extra 10 pixels of height.
    int idxLastLyrics = -1;

    for (int i = 0; i < score->nstaves(); i++) {
        const Staff* staff = score->staff(i);

        // Visible staves
        visibleStaves[i] = staff->part()->show() ? nVisible++ : -1;

        // Non-standard staves
        if (staff->isDrumStaff() || staff->isTabStaff()) {
            nonStdStaves.push_back(i);
            if (isMulti && staff->isTabStaff())
                hasTabs = true; // for auto-export of fretboards later
        }

        // Last lyrics staff !!!bool Staff::hasLyrics() would be a good thing
        Segment::Type st = Segment::Type::ChordRest;
        for (Segment* seg = score->firstMeasureMM()->first(st); seg; seg = seg->next1MM(st)) {
            ChordRest* cr = seg->cr(i * VOICES);
            if (cr && !cr->lyrics().empty()) {
                idxLastLyrics = i;
                break;
            }
        }
    }
    printer.setNStaves(nVisible);
    printer.setNonStandardStaves(&nonStdStaves);

    // The sort order for elmPtrs is critical: if (isMulti) by type, by staff;
    //                                         else         by type;
    QList<Element*> elmPtrs = page->elements();
    std::stable_sort(elmPtrs.begin(), elmPtrs.end(), elementLessThan);
    if (isMulti)
        std::stable_sort(elmPtrs.begin(), elmPtrs.end(), elementLessThanByStaff);
    else // Paint staff lines once, prior to painting anything else
        paintStaffLines(score, &p, &printer, page, &visibleStaves);

    QStringList    iNames;    // Only used by Multi-Select Staves.
    QList<qreal>   staffTops; // ditto
    int            idxStaff;  // Everything is grouped by staff.
    EType          eType;     // Everything is determined by element type.
    const Segment* seg;        // It has start and end ticks for highlighting.

    int maxNote = 0; // data-maxnote = Max note duration for this score. Helps optimize
                     // highlighting previous notes when user changes start time.
    idxStaff = -1;
    Beam* b;
    foreach (const Element* e, elmPtrs) {
        // Always exclude invisible elements from this pass, except TEMPO_TEXT.
        if (!e->visible() && eType != EType::TEMPO_TEXT)
                continue;

        // Multi-Select Staves groups and draws staves one at a time
        const int idx = e->staffIdx();
        if (isMulti && idxStaff != idx) {
            if (idxStaff > -1) {
                const int lyricsHeight = (idxStaff != idxLastLyrics ? 20 : 25);
                // Paint the previous staff's animated elements
                paintStaffSMAWS(score, &p, &printer, &mapFrozen, &mapSVG, &mapLyrics,
                                &visibleStaves, &staffTops, idxStaff, isMulti, lyricsHeight);
                mapFrozen.clear();
                mapLyrics.clear();
                mapSVG.clear();
            }
            // We're starting s new staff, paint its staff lines
            paintStaffLines(score, &p, &printer, page, &visibleStaves, nVisible,
                            idx, isMulti, &iNames, &staffTops, &setVTT);
            idxStaff = idx;
        }

        // Paint inanimate elements and collect animation elements.
        seg = 0;
        eType = e->type();
        switch (eType) {
        case EType::STAFF_LINES :   /// Not animated, but handled previously.
            continue;
            break;
                                        /// Highlighted Elements:
        case EType::REST       : //                = ChordRest subclass Rest
        case EType::LYRICS     : //        .parent = ChordRest
        case EType::NOTE       : //        .parent = ChordRest
        case EType::NOTEDOT    : // .parent.parent = ChordRest subclass Chord
        case EType::ACCIDENTAL : // .parent.parent = ChordRest subclass Chord
        case EType::STEM       : //         .chord = ChordRest subclass Chord
        case EType::HOOK       : //         .chord = ChordRest subclass Chord
        case EType::BEAM       : //      .elements = ChordRest vector
        case EType::HARMONY    : //     annotation = handled by getAnnCueId()
            switch (eType) {
            case EType::REST :
                seg = static_cast<const Segment*>(e->parent());
                break;
            case EType::LYRICS :
            case EType::NOTE   :
                seg = static_cast<const Segment*>(e->parent()->parent());
                maxNote = qMax(maxNote, seg->ticks());
                break;
            case EType::NOTEDOT    :
            case EType::ACCIDENTAL :
                seg = static_cast<const Segment*>(e->parent()->parent()->parent());
                break;
            case EType::STEM :
                seg = static_cast<const Segment*>(static_cast<const Stem*>(e)->chord()->parent()->parent());
                break;
            case EType::HOOK :
                seg = static_cast<const Segment*>(static_cast<const Hook*>(e)->chord()->parent()->parent());
                break;
            case EType::BEAM : // special case: end tick is last note beamed
                //This is because a const Beam* cannot call ->elements()!!!
                b = static_cast<const Beam*>(e)->clone();
                // Beam cue runs from first element to last
                cue_id = getCueID(b->elements()[0]->tick(),
                                  b->elements()[b->elements().size() - 1]->tick());
                break;
            case EType::HARMONY : // special case: end tick is next HARMONY
                cue_id = getAnnCueID(score, e, eType);
                break;
            default:
                break; // should never happen
            }
            if (seg != 0) // exclude special cases
                cue_id = getCueID(seg->tick(), seg->tick() + seg->ticks());

            setVTT.append(cue_id);
            if (isMulti) {
                if(eType == EType::HARMONY)
                    mapSysStaff.insert(cue_id, e);  // "system" staff
                else if(eType == EType::LYRICS)
                    mapLyrics.insert(cue_id, e); // lyrics get their own pseudo-staff
            }
            else
                mapSVG.insert(cue_id, e);
            continue;
            break;

        case EType::BAR_LINE       :    /// Ruler/Scrolling elements
        case EType::REHEARSAL_MARK :
            // Add the cue ID to the VTT set.
            cue_id = getScrollCueID(score, e);
            setVTT.append(cue_id);
            if (isMulti && eType == EType::REHEARSAL_MARK) {
                mapSysStaff.insert(cue_id, e);
                continue;
            }
            break;

        case EType::TEMPO_TEXT        : /// Frozen Pane elements
        case EType::INSTRUMENT_NAME   :
        case EType::INSTRUMENT_CHANGE :
        case EType::CLEF    :
        case EType::KEYSIG  :
        case EType::TIMESIG :
            if (!isScrollVertical) {
                cue_id = getScrollCueID(score, e); // Zero-duration cue
                setVTT.append(cue_id);
                if (isMulti && eType == EType::TEMPO_TEXT)
                    mapSysStaff.insert(cue_id, e);
                else
                    mapFrozen.insert(cue_id, e);
                continue;
            }
            else
                cue_id = ""; // vertical scrolling: these elements not animated
            break;

        case EType::TEXT       :        /// Assorted other elements
        case EType::STAFF_TEXT : {
            TextStyleType tst = static_cast<const Text*>(e)->textStyleType();
            if (tst == TextStyleType::MEASURE_NUMBER) { // zero-duration cue, no highlight, but click
                cue_id = getCueID(static_cast<const Measure*>(e->parent())->first()->tick());
                setVTT.append(cue_id);
                if (isMulti)
                    mapSysStaff.insert(cue_id, e);
                else
                    mapSVG.insert(cue_id, e);
                continue;
            }
            else if (isMulti && tst == TextStyleType::SYSTEM) { // add to "system" staff, but no cue
                mapSysStaff.insert("", e);
                continue;
            }
            break;}

        case EType::LYRICSLINE_SEGMENT :
            if (isMulti) { // add to lyrics staff, but no cue
                mapLyrics.insert("", e);
                continue;
            } // fall-through is by design, no break here
        default:                        /// Un-animated (inanimate?) elements
            cue_id = "";
            break;
        }

        // Set the Element pointer inside SvgGenerator/SvgPaintEngine
        printer.setElement(e);

        // Custom data-start attribute = start time in whole milliseconds.
        // Elements with the onClick event need it. Yes, it semi-duplicates
        // the cue id, but not totally. Some elements have a cue_id, but no
        // data-start attribute, because they are not clickable in the score.
        // RehearsalMarks have a data-start, but no cue id, because I prefer
        // not to highlight them in the score (at least for now), but they
        // are clickable in the Markers timeline ruler.
///!!!OBSOLETE!!!        printer.setStartMSecs(startMSecsFromCueID(score, cue_id));

        // Set the cue_id, even if it's empty (especially if it's empty)
        printer.setCueID(cue_id);

        // Paint the (un-animated) element
        paintElement(p, e);

    } // for (e. elmPtrs)

    // Multi-Select Staves
    if (isMulti) {
        // Paint the last staff's animated elements
        paintStaffSMAWS(score, &p, &printer, &mapFrozen, &mapSVG, &mapLyrics,
                        &visibleStaves, &staffTops, idxStaff, isMulti);

        // Paint the staff-independent elements
        // No staff lines, no bar lines, so no call to paintStaffLines()
        iNames.append("system");
        printer.setStaffIndex(nVisible); // only affects fancy formatting
        printer.setYOffset(0);
        printer.beginMultiGroup(&iNames, "system", "system", 35, 0, QString()); ///!!! 35 is standard top staff line y-coord, I'm being lazy here by hardcoding it
        for (SVGMap::iterator i = mapSysStaff.begin(); i != mapSysStaff.end(); ++i) {
            cue_id = i.key();
///!!!OBSOLETE!!!            printer.setStartMSecs(startMSecsFromCueID(score, cue_id));
            printer.setCueID(cue_id);
            printer.setElement(i.value());
            paintElement(p, i.value());
        }
        printer.endMultiGroup();

        // Multi-Select Staves frozen pane has <use> elements, one per staff
        staffTops.append(staffTops[0]); // For the staff-independent elements
        for (int i = 0; i < iNames.size(); i++)
            printer.createMultiUse(staffTops[i] - staffTops[0]);
    }
    else {
        // Paint everything all at once, not by staff
        paintStaffSMAWS(score, &p, &printer, &mapFrozen, &mapSVG, &mapLyrics, &visibleStaves);

        // These go in the <svg> element for the full score/part
        printer.setCueID(getGrayOutCues(score, -1, &setVTT));
    }

    // Max note duration, in ticks, for the inefficient task of highlighting
    // previous notes when the user change the start time.
    printer.setMaxNote(maxNote);

    // The end of audio is a cue that equals the end of the last bar.
    setVTT.append(getCueID(score->lastSegment()->tick()));

    // Write the VTT file
    if (!saveVTT(score, fnRoot, setVTT))
        return false;

    // Finish sheet music export and clean up
    score->setPrinting(false);
    MScore::pdfPrinting = false;
    p.end(); // Writes MuseScore (and Frozen) SVG file(s) to disk, finally

    // If there is tablature and isMulti, export fretboards too
    if (isAuto && hasTabs)
        saveSMAWS_Frets(score, qfi);

    return true;
}

// streamRulers()
// Streams the Bars and Markers rulers. Consolidates code for internal and
// external (separate file) rulers. This is a tedious function, with lots
// of constant values. It streams the bars and markers rulers separately.
//
// In MuseScore, a Measure's Barline is the line at the end of the bar.
// No one thinks of bar numbers as the end-barline. If I click on a barline
// in the bars ruler, I expect to hear the start of that bar, not the end.
// This means there's an extra bar # in every score's bars ruler for the
// last bar that has no music, but is the end of the song.
// First/last ruler line text is left/right aligned, not centered.
//
// Deal with bars ruler line density? No. The more bars per score, the less
// space between lines. For scores beyond a certain length, the ruler will
// need to have 1 line for every 2 or more bars. For now, my scores are
// not long enough to reach this limit, so I'm not going to code for it.
// For now: the bars ruler is 1:1, lines:bars.
//
// Short vs long lines every how many bars? Long every 5, Label every 10.
// This is reflected in the value of a line's y2 attribute.
static void streamRulers(Score*         score,
                         QFileInfo*     qfi,
                         QTextStream*   streamBars,
                         QTextStream*   streamMarks,
                         IntSet*        setVTT,
                         int            width,
                         const QString& evtPrefix,
                         const QString& indent = "")
{
    // Strings for the ruler's <line> and <text> element attributes
    QString label; // <text> element contents

    QString y;     // y = one of these two values, depending on element type
    const QString yBars  = " y=\"18\"";
    const QString yMarks = " y=\"17\"";

    // Ruler lines don't start at zero or end at width, left=right margin
    const int margin =  8; // margin on either side of ruler
    int    cntrWidth = 80; // counter width

    // Score::duration() returns # of seconds as an int, I need more accuracy
    const TempoMap* tempos   = score->tempomap();
    const qreal     duration = tempos->tick2time(score->lastMeasure()->tick()
                                               + score->lastMeasure()->ticks());

    const int barNoDigits = QString::number(score->lastMeasure()->no()).size();

    // Pixels of width per millisecond, left + right margins
    const qreal pxPerMSec = (width - cntrWidth - (margin * 2)) / (duration * 1000);

    // For CSS Styling
    QString lineClass;
    QString textClass;
    const QString classRul  = "ruler\" ";
    const QString classRul5 = "ruler5\"";
    const QString classMrk  = "marker\"";

    // Ruler event handlers
    const QString rulerMouse = QString(" onclick=\"%1rulerClick(evt)\" onmouseover=\"%2rulerOver(evt)\" onmouseout=\"%3rulerOut(evt)\" onmouseup=\"%4rulerUp(evt)\"").arg(evtPrefix).arg(evtPrefix).arg(evtPrefix).arg(evtPrefix);

    // Collect the ruler elements
    // A multimap because a barline and a marker can share the same tick
    QMultiMap<int, Element*> mapSVG;
    int tick;

    Measure* m;
    Segment* s;
    for (m = score->firstMeasure(); m; m = m->nextMeasureMM())
    {
        // Bars ruler by Measure
        tick = m->tick();
        mapSVG.insert(tick, static_cast<Element*>(m));
        setVTT->insert(tick);

        // Markers ruler by Rehearsal Mark, which is effectively by Segment
        for (s = m->first(Segment::Type::ChordRest); s; s = s->next(Segment::Type::ChordRest))
        {
            for (Element* eAnn : s->annotations()) {
                if (eAnn->type() == EType::REHEARSAL_MARK) {
                    tick = s->tick();
                    mapSVG.insert(tick, eAnn);
                    setVTT->insert(tick);
                    break; // only one marker per segment
                }
            }
        }
    }

    const int xDigits = 4; // x-coord never greater than 9999
    int   iY1, iY2; // Integer version of y1 and y2 coordinates
    int   iBarNo;   // Integer version of measure number
    qreal pxX;      // Floating point version of x coordinates
    qreal offX;     // x offset for rehearsal mark text

    // For the invisible, but clickable, rects around lines
    const int halfHeight = qFloor(RULER_HEIGHT / 2); // RULER_HEIGHT is an odd number
    qreal   rectX = cntrWidth + 1;
    qreal   rectWidth;
    qreal   lineX; // The previous bar ruler line x-coord


    // End of music tick is required in VTT too
    tick = score->lastSegment()->tick(); // for endRect and loopEnd
    setVTT->insert(tick);

    // Each ruler is a <g> element with a fixed set of common elements. Those
    // common elements are configured in a file, with replacement slots as %N.
    QFile       qf;
    QTextStream qts;
    QTextStream qtsFile;
    QString     qs;
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_RULER_G));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qtsFile.setDevice(&qf);
    qts.setString(&qs);

    // Read the file, replacing text that's the same in both rulers
    qts << qtsFile.readAll().replace("%10", QString::number(cntrWidth + 1))
                            .replace("%11", QString::number(width - cntrWidth - 2))
                            .replace("%12", QString::number(width - margin))
                            .replace("%13", QString::number(margin - 1))
                            .replace("%14", QString::number(tick))
                            .replace("%15", evtPrefix)
                            .replace("%16", QString::number(cntrWidth + margin + 1));

    // Stream to each ruler with it's specific text
    *streamBars  << QString(qs).arg(halfHeight).arg(halfHeight).arg(halfHeight)
                     /* %4 */  .arg(halfHeight - 1)
                     /* %5 */  .arg(SVG_ZERO)
                     /* %6 */  .arg((cntrWidth / 2) + 1)
                     /* %7 */  .arg("cntrBars")
                     /* %8 */  .arg(QString("Bar %1").arg(score->firstMeasureMM()->no() + 1, 3, int(10), QLatin1Char(SVG_ZERO)))
                     /* %9 */  .arg("-6,1 6,1 0,15");
    *streamMarks << QString(qs).arg(halfHeight - 1).arg(halfHeight - 1).arg(halfHeight - 1)
                     /* %4 */  .arg(halfHeight - 2)
                     /* %5 */  .arg(SVG_ONE)
                     /* %6 */  .arg(cntrWidth / 2)
                     /* %7 */  .arg("cntrTime")
                     /* %8 */  .arg("00:00")
                     /* %9 */  .arg("-6,19 6,19 0,5");

    cntrWidth++;
    const QString loopStart = QString("<polygon class=\"cursLoopLo\" points=\"-6,1 6,1 0,18\" transform=\"translate(%1,0)\" onmousedown=\"%2cursorDown(evt)\" onmouseup=\"%3cursorUp(evt)\"/>\n").arg(margin + cntrWidth).arg(evtPrefix).arg(evtPrefix);
    const QString loopEnd   = QString("<polygon class=\"cursLoopLo\" points=\"-6,19 6,19 0,2\" transform=\"translate(%1.5,0)\" onmousedown=\"%2cursorDown(evt)\" onmouseup=\"%3cursorUp(evt)\"/>\n").arg(width - margin).arg(evtPrefix).arg(evtPrefix);

    // Display floating point numbers with consistent precision
    streamBars->setRealNumberPrecision(SVG_PRECISION);
    streamBars->setRealNumberNotation(QTextStream::FixedNotation);
    streamMarks->setRealNumberPrecision(SVG_PRECISION);
    streamMarks->setRealNumberNotation(QTextStream::FixedNotation);

    // Stream the line and text elements, with all their attributes:
    //   cue_id, x, x1, x2, y, y1, y2, and label
    // y and y1 are fixed by element type. That leaves:
    //   cue_id, x, x1, x2, y2, and label
    int rectCue;  // bars ruler rects apply to the previous bar
    int prevCue;
    for (QMultiMap<int, Element*>::iterator i  = mapSVG.begin();
                                            i != mapSVG.end();
                                            i++) {
        qreal   x;
        Element*  e = i.value();
        EType eType = e->type();

        // Default Values:
        // x = x1 = x2, they're all the same: a vertical line or centered text.
        // The exception is x for rehearsal mark text, which is offset right.
        // Y values are varied, but with a limited set of values, by EType.
        iY1    = 1;
        iY2    = 9;
        offX   = 0;
        lineClass = classRul;

        // Values for this cue
        tick = i.key();
        pxX = cntrWidth + margin + (pxPerMSec * startMSecsFromTick(score, tick));

        switch (eType) {
        case EType::MEASURE :
            iBarNo = static_cast<Measure*>(e)->no() + 1;
            if (iBarNo % 5 == 0) {
                // Multiples of 5 get a longer, thick line
                iY2 = 13;
                lineClass = classRul5;
                textClass = classRul5;
                if (iBarNo % 10 == 0) {
                    // Multiples of 10 get text and a shorter, thick line
                    iY2 = 4;
                    label = QString("%1").arg(iBarNo);
                }
            }
            else
                label  = "";

            if (tick > 0) {
                x = rectX;
                rectWidth = pxX - ((pxX - lineX) / 2) - rectX;
                rectCue= prevCue;
            }
            y     = yBars;
            break;
        case EType::REHEARSAL_MARK :
            y     = yMarks;
            iY1   =  7;
            iY2   = halfHeight - 2;
            offX  =  6;
            x     = pxX - offX;
            label = static_cast<const Text*>(e)->xmlText();
            lineClass = classRul5;
            textClass = classMrk;
            rectWidth = offX * 2;
            rectCue   = tick;
            break;

        default:
            break;
        }

        const bool  isMarker = (eType == EType::REHEARSAL_MARK);
        QTextStream* streamX = isMarker ? streamMarks : streamBars;

        // Ruler lines have invisible rects around them, allowing users to be
        // less precise with their mouse clicks. These rects must be after
        // the line/text, because for Bars, those lines/texts have no events.
        // The code operates on the current Marker and the previous BarLine.
        // Marker rects have a fixed width rect = text offset from line.
        // BarLine rects split the space around each line, no empty spaces.
        if (isMarker || tick > 0) {
            *streamX << indent << SVG_RECT
                     << formatInt(SVG_CUE_NQ, rectCue, CUE_ID_FIELD_WIDTH, true)
                     << formatReal(SVG_X, x, xDigits, true)
                     << SVG_Y  << SVG_QUOTE << 1 - (isMarker ? 0 : halfHeight) << SVG_QUOTE
                     << SVG_WIDTH  << rectWidth << SVG_QUOTE
                     << SVG_HEIGHT << (isMarker ? halfHeight : RULER_HEIGHT) - 2 << SVG_QUOTE
                     << SVG_FILL   << SVG_NONE  << SVG_QUOTE
                     << rulerMouse
                     << SVG_ELEMENT_END << endl;

            if (!isMarker) // tick > 0
                rectX += rectWidth;
        }

        // Both Markers and Bars get the line and, conditionally, text.
        *streamX << indent << SVG_LINE
                 << formatInt(SVG_CUE_NQ, tick, CUE_ID_FIELD_WIDTH, true)
                 << (isMarker ? "" : formatInt(SVG_BARNUMB, iBarNo, barNoDigits, true))
                 << SVG_CLASS  << lineClass
                 << SVG_Y1     << iY1    << SVG_QUOTE
                 << SVG_Y2     << iY2    << SVG_QUOTE
                 << formatReal(SVG_X1_NQ, pxX, xDigits, true) // vertical lines
                 << formatReal(SVG_X2_NQ, pxX, xDigits, true) //    x1 == x2
                 << (isMarker ? rulerMouse : "")
                 << SVG_ELEMENT_END << endl;

        // Only stream the text element if there's text inside it
        if (!label.isEmpty()) {
            *streamX << indent << SVG_TEXT_BEGIN
                     << formatInt(SVG_CUE_NQ, tick, CUE_ID_FIELD_WIDTH, true)
                     << SVG_CLASS << textClass
                     << formatReal(SVG_X, pxX + offX, xDigits, true)
                     << y
                     << (isMarker ? rulerMouse : "") << SVG_GT
                     << label
                     << SVG_TEXT_END << endl;
        }

        if (!isMarker) { // Bars only
            lineX = pxX;
            prevCue = tick;
        }
    } //for (i)

    // Invisible <rect> for final bar line
    rectWidth = width - rectX - 1;
    *streamBars << indent << SVG_RECT
                << formatInt(SVG_CUE_NQ, tick, CUE_ID_FIELD_WIDTH, true)
                << formatReal(SVG_X, rectX, xDigits, true)
                << SVG_Y   << SVG_QUOTE << iY1 - halfHeight   << SVG_QUOTE
                << SVG_WIDTH            << rectWidth - margin << SVG_QUOTE
                << SVG_HEIGHT           << RULER_HEIGHT - 2   << SVG_QUOTE
                << SVG_FILL             << SVG_NONE           << SVG_QUOTE
                << rulerMouse
                << SVG_ELEMENT_END << endl;

    // The loop cursors
    *streamMarks << indent << loopStart;
    *streamBars  << indent << loopEnd;
}

// MuseScore::saveSMAWS_Rulers()
//-Generates the BarLine and RehearsalMarker rulers files for a score.
// This is done once per composition, not for each part. The VTT is generated
// by saveSMAWS_Music() when exporting each part and/or score for that composition.
//-This only generates SVG files. The rulers' VTT cues are generated/stored in
// the score's VTT files. A separate rulers VTT file has not been necessary.
bool MuseScore::saveSMAWS_Rulers(Score* score, QFileInfo* qfi)
{
    if (score->metaTag(tagWorkNo).isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Rulers"), tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }

    IntSet setVTT; // Start-time-only cues

    const int wRuler = 1919; // Width of buttons + rulers + counters = 1920(HD width) - 1, cuz that's how it works with browser maximized
    const int wButts =   91; // Width of the playback buttons

    // Event handler for clicking on ruler lines/text. Note the "top." prefix
    // to the function name. It's calling the (HTML) container's function.
    const QString evtPrefix = "top.";

    // The root file name, without the .ext
    const QString fnRoot = QString("%1/%2%3%4").arg(qfi->path())
                                               .arg(score->metaTag(tagWorkNo))
                                               .arg(SMAWS_)
                                               .arg(SMAWS_RULERS);

    // 2 streams = 2 <g> elements in 1 SVG file
    QFile       rulersFile;
    QTextStream fileStream(&rulersFile);
    rulersFile.setFileName(QString("%1%2").arg(fnRoot).arg(EXT_SVG));
    rulersFile.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!

    // Boilerplate header/title, configured in an external file
    QFile       qf;
    QTextStream qts;
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_RULER_HDR));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);
    fileStream << qts.readAll().replace("%1", QString::number(wRuler))
                               .replace("%2", QString::number(RULER_HEIGHT))
                               .replace("%3", score->title());

    // Stream the fixed playback buttons from a file
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_PLAY_BUTTS));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);
    fileStream << qts.readAll();

    // streamRulers() takes separate streams for bars/markers
    // Bars goes first because it's full-height (38px) invisible rects are
    // behind the Markers elements - all for mouse event management.
    QString markers;
    QTextStream markStream(&markers);
    fileStream << SVG_GROUP_BEGIN  << SVG_ID        << "bars"
               << SVG_QUOTE        << SVG_TRANSFORM << SVG_TRANSLATE
               << wButts           << SVG_SPACE     << qFloor(RULER_HEIGHT / 2) // This gives the extra pixel to the bars ruler
               << SVG_RPAREN_QUOTE << SVG_GT        << endl;

    streamRulers(score, qfi, &fileStream, &markStream, &setVTT, wRuler - wButts, evtPrefix, SVG_4SPACES);

    fileStream << SVG_GROUP_END    << endl          << endl
               << SVG_GROUP_BEGIN  << SVG_ID        << "markers"
               << SVG_QUOTE        << SVG_TRANSFORM << SVG_TRANSLATE
               << wButts           << SVG_SPACE     << SVG_ZERO
               << SVG_RPAREN_QUOTE << SVG_GT        << endl
               << markers          << SVG_GROUP_END << endl << endl;

    // Stream the "footer", terminating the <svg> element
    fileStream  << SVG_END;

    // Write/close the SVG file
    fileStream.flush();
    rulersFile.close();

    // VTT file, start times only
    saveStartVTT(score, fnRoot, &setVTT);

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// saveSMAWS_Tables() - SMAWS HTML & SVG Grid (Drum Machine Style) generator
//
// A single score can have multiple tables, just as a single HTML page can
// have multiple tables.  In MuseScore I'm using the repeat barlines as a
// 1)separator and
// 2)loop vs. pick-up/count-off indicator.
// There are three types of repeat barlines: Left, Right, and Both.
// This code uses the Measure class's have a Repeat Flag to get that data.

// Pitch is 100% irrelevant at this time, as are multiple notes/voices in
// a staff's ChordRest. All that matters is: note duration per staff.
// Pitch and more could definitely become useful when this gets hooked up
// to MIDI on the client end. Not yet...

// I handle pick-up measures (Repeat::NONE), but not let-downs. No need yet.

// Every SMAWS HTML Table's columns are defined by the length of the repeat
// segment and the grid density. The grid staff has lyrics, which are
// displayed as grid text. I'm using the convention that staves with lyrics
// have the Small staff checkbox checked.
// The lyrics staff with the partName == "grid" is the grid staff.
// The lyrics staff with the partName == "chords" is an optional chords staff.
//
// This grid staff contains notes, no rests, for the length of each repeat.
// Anywhere you want to make a table column, put a note in the grid staff.
// Generally within a table, the grid is one duration (e.g. 1/8th notes)
// for the entire table.  But it is plausible in a Chords table to need a
// mainly 1-bar grid that has 1/2 bar columns for mid-bar chord changes.
// If barline borders are different than intra-bar borders, this is OK.
// Sub-dividing the grid with extra <td>s in a column doesn't work easily.
// Implementation is a colspan in the grid row + extra column(s) in table.
// I'm not implementing it (yet). No pressing need, may never arise...
// For now sub-divisions of the grid by staff are considered an error.

// The order of the staves top-to-bottom == the order of the rows
// Staff == Row, Time/Beat == Column
// Extra header column for intrument names (row headers)
// Extra pseudo-header columns for pick-up/count-off

// For SVG, x-axis = columns, y-axis = rows: Columns move left-to-right
//                                              Rows move top-to-bottom
// It's totally backwards from the standard spreadsheet row/column concept.
// Think about it this way: row data is across the columns
//                          col data is across the rows
// It gets into reverse translating from data cells to for() loop iteration.
//
// Don't be confused by isChord vs. idxChords:
//     isChord   == bool: if (true) this ChordRest* is a Chord* not a Rest*
//     idxChords == int: staff index for optional chord changes, EType::HARMONY
//
bool MuseScore::saveSMAWS_Tables(Score*     score,
                                 QFileInfo* qfi,
                                 bool       isHTML,
                                 bool       hasRulers)
{
    if (score->metaTag(tagWorkNo).isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Tree"), tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }

    // Total staves, including invible and temporarily invisible staves
    const int nStaves = score->nstaves();

    // Iterate by Staff to locate the required grid staff and optional chords staff
    const QString CHORDS = "chords";
    int     idxStaff;
    int     idxGrid   = -1; // The index of the grid staff/row
    int     idxChords = -1; // The index of the optionsl chords row
    IntList twoVoices;      // For 2-voice lyrics staves

    for (idxStaff = 0; idxStaff < nStaves; idxStaff++) {
        if (score->staff(idxStaff)->small()) {
            if (score->staff(idxStaff)->partName() == STAFF_GRID)
                idxGrid = idxStaff;
            else if (score->staff(idxStaff)->partName() == CHORDS)
                idxChords = idxStaff;
            else if (score->staff(idxStaff)->cutaway()) // for lack of a custom property
                twoVoices.append(idxStaff);
        }
    }
    if (idxGrid < 0)
        return false; // no grid staff == no good

    int startOffset = 0; // this repeat's  start tick
    int startTick   = 0; // this chord's   start tick, relative to this repeat
    int mStartTick  = 0; // this measure's start tick, relative to this repeat
    int mTicks      = 0; // this measure's     actual ticks (duration)
    int dataTicks   = 0; // this chord's       actual ticks
    int gridTicks   = 0; // this grid column's actual ticks (duration)
    int gridTick    = 0; // this grid column's un-offset tick value = cr.tick()
    int pageTick    = 0; // this page's endTick == next page's mStartTick
    int idxPage     =-1; // this page's page index (page number minus one)
    int idxCol      = 0; // if (isPages) the current column index
    int idxBar      = 0; // ditto, bar line index
    int idxBeat     = 0; // ditto, grid line index
    int nTables     = 0; // if (tableTitle.isEmpty()) unique, numbered titles;

    Measure*   m1;       // the first measure in the table
    Measure*   m;        // this measure
    Segment*   s;        // this segment
    ChordRest* crGrid;   // this start tick's ChordRest from the grid staff
    ChordRest* crData;   // this start tick's ChordRest from this staff
    Note*      note;     // if crData is a chord, its first note
    bool       isChord;  // is crData a chord?
    bool       isLED;    // is this staff a LED (notes) or Lyrics (text) staff?

    QTextStream     qts;        // temp variable used to populate grid and more
    QString*        pqs;        //  ditto
    StrPtrList*     spl;        //  ditto
    StrPtrVect*     spv;        //  ditto
    StrPtrListVect* splv;       //  ditto...
    IntList*        pil;        // temp variable for pitches/ordinals
    IntListVect*    pilv;       // ditto
    QString         cue_id;     // temp variable for cue_id
    QString         page_id;    // ditto for page cue_id
    QString         tableTitle; // RehearsalMark at startTick==0 is the title

    QStringList    tableCues; // List of cue_ids for VTT file
    StrPtrList     barNums;   // List of <text>s
    StrPtrList     barLines;  // List of <line>s
    StrPtrList     beatLines; // Every whole beat (1/4 note in 4/4 time)
    StrPtrList     gridUse;   // if (isPages) this is for grid staff
    StrPtrList     gridText;  //  ditto
    StrPtrVectList grid;      // List = columns/beats. Vector = rows/staves
    StrPtrVectList dataCues;  // data-cue only in cells that have >0 cue_ids

    // Tempo
    qreal   initialBPM;     // initial tempo in BPM
    qreal   prevTempo;      // previous tempo in BPS
    int     prevTempoPage;  // previous tempo in BPS
    QString tempoCues;      // data-cue="id1;BPM1,id2:BPM2,..." for tempo changes

    const int  BPM_PRECISION = 2;
    const TempoMap* tempoMap = score->tempomap(); // a convenience

    // By page
    IntList        pageCols;     // active column count per page
    StrPtrListList pageGridText; // grid row innerHTML, by column
    StrPtrVectList pageNames;    // instrument name text, by row
    StrPtrVectList pageStyles;   // instrument name style: Lo or No, by row
    StrPtrListVectList leds;     // data cell <use> href> or <text> lyrics: by col, by row, by page
    IntListVectList    pitches;  // MIDI note number by col, by row, by page

    IntListList    pageBeats;   // x-coordinates by beatline
    IntListList    pageBars;    // x-coordinates by barline
    IntListList    pageBarNums; // bar numbers   by barline

    IntListVect  pitchSet(nStaves, 0); // by row: sorted, unique set of MIDI note numbers
    StrPtrVect     iNames(nStaves, 0); // Row header instrument names

    // Pages are repeated patterns within a repeat ~= voltas.
    // Pages are separated by double-barlines.
    // Pages are SVG only.
    // These variables are all part of multi-page functionality:
    bool isPages     = false;
    bool isPageStart = false;
    BoolVect    isStaffVisible(nStaves, true);
    Measure*    mp;       // temp variable for measure within a page
    Measure*    mPageEnd; // this page's end measure
    QString     pageCues; // page #: data-cue="id1,text1;id2,text2;etc."
    QStringList pageIDs;  // just the page_ids, in order, by page

    // For 2-voice lyrics staves
    bool           isChord2;
    int            dataTicks2 = 0;
    QString        cue_id2;
    QTextStream    qts2;
    QString*       pqs2;
    StrPtrVect*    spv2;
    ChordRest*     cr2;
    Note*          note2;
    StrPtrVectList grid2;
    StrPtrVectList dataCues2;
    StrPtrListVectList leds2;

    // Constants for SVG table cell dimensions, in pixels
    const int cellWidth  =  48;
    const int cellHeight =  48;
    const int baseline   = cellHeight - 14; // text baseline for lyrics/instrument names
    const int nameLeft   =  19; // this value is duplicated in FILE_GRID_INST
    const int iNameWidth = 144; // Instrument names are in the left-most column
    const int maxDigits  =   4; // for x/y value formatting, range = 0-9999px

    // Variables for SVG table cell positions
    int cellX  = iNameWidth;    // fixed offset for instrument name row headers
    int cellY  = 0;
    int height = 0;
    int width  = 0;

    // These are only used for barLines <rect>s and beatLines <line>s
    const int   gridMargin =  8;
    const int   barMargin  = 11;
    const int   barWidth   =  2; // <rect> acting as a bar line
    const qreal barRound   =  1; // barWidth/2 simulates stroke-linecap:round

    // For ledMini and multi-pitch rows
    const qreal ledHeight     = 40.0;
    const qreal miniHeight    = 17.0;
    const qreal verticalSpace = ledHeight - miniHeight;
    const qreal restOffset    = (ledHeight / 2) - (miniHeight / 2);

    // For xlink:href values
    const QString LED  = "led";
    const QString MINI = "ledMini";
    const QString LO   = "Lo";
    const QString NO   = "No";

    // if (!hasRulers) event functions are in the parent document's scripts
    const QString evtPrefix = (hasRulers ? "" : "top.");

    // Currently two HTML table styles, one kludgy option implementation.
    // Two styles are: tableChords and tableDrumMachine
    const QString classHTML = score->pageFormat()->twosided()
                            ? "tableChords"
                            : "tableDrumMachine";

    // Creates unique title if the score has none: << idStub << ++nTables
    // Each HTML table gets a unique id value: idStub1, idStub2, etc. !!! for now... Is it necessary?
    const QString idStub = "tblSMAWS";

    // qfi is a VTT file, this needs an SVG or HTML file too
    const QString fnRoot = QString("%1/%2%3%4").arg(qfi->path())
                                               .arg(score->metaTag(tagWorkNo))
                                               .arg(SMAWS_)
                                               .arg(SMAWS_GRID);
    QString       fnTable;
    QFile         tableFile;
    QTextStream   tableStream;

    // HTML: One file contains multiple tables
    if (isHTML) {
        // Open a stream into the HTML file
        fnTable = QString("%1%2").arg(fnRoot).arg(EXT_HTML);
        tableFile.setFileName(fnTable);
        tableFile.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
        tableStream.setDevice(&tableFile);

        // Stream the <head> and the start of the <body>
        tableStream << HTML_HEADER;
    }

    // Iterate sequentially, chronologically, left-to-right:
    //   by measure, by segment of type ChordRest, if grid staff contains chord
    for (m = score->firstMeasure(); m; m = m->nextMeasureMM())
    {
        // Start-of-repeat or pickup bar
        // Every repeat and every pickup effectively start at tick zero
        if (m->repeatStart())
            startOffset = m->tick();

        mStartTick = m->tick() - startOffset;
        mTicks     = m->ticks();

        // For bars ruler, every start-of-bar is a cue
        tableCues.append(getCueID(mStartTick));

        for (s = m->first(Segment::Type::ChordRest); s; s = s->next(Segment::Type::ChordRest))
        {
            crGrid = s->cr(idxGrid * VOICES); // The grid staff's ChordRest
            if (crGrid == 0)
                continue;

            if (crGrid->type() == EType::CHORD) {                              /// GRID CHORD ///
                if (gridTicks == 0                      //    new table
                || (isPages && pageTick == mStartTick)) // or new page
                {
                    if (gridTicks == 0) {                                      /// NEW TABLE ///
                        // The table's title is the grid staff's first staff text
                        for (Element* eAnn : s->annotations()) {
                            if (eAnn->type() == EType::STAFF_TEXT) {
                                tableTitle = static_cast<Text*>(eAnn)->xmlText();
                                break;
                            }
                        }
                        if (tableTitle.isEmpty()) // Numerically indexed title
                            tableTitle = QString("%1%2").arg(idStub).arg(++nTables);

                        // The initial bpm
                        prevTempo     = tempoMap->tempo(m->tick());
                        initialBPM    = prevTempo * 60;
                        prevTempoPage = 0;

                        m1 = m; // this is useful later
                    }

                    if (isHTML) { // One pages for all the tables, one file too
                        // Stream the start of the <table> element to the file
                        tableStream << HTML_TABLE_BEGIN
                                    << SVG_ID    << tableTitle.replace(SVG_SPACE, SVG_DASH) << SVG_QUOTE
                                    << SVG_CLASS << classHTML                               << SVG_QUOTE
                                    << SVG_GT    << endl;
                    }
                    else { // SVG:\\ Deal with multiple pages, or not...
                        // Find the next double-barline or Repeat::END
                        for (mp = m; mp; mp = mp->nextMeasureMM()) {           /// ¿NEW PAGE? ///
                            if (mp->repeatEnd()) {
                                if (isPages)            // end of table:
                                    isPageStart = true; //     only one page
                                break;                  //     or  last page
                            }
                            else if (mp->endBarLineType() == BarLineType::DOUBLE) {
                                isPages     = true;   // this table has pages
                                isPageStart = true;   // this cr is start-of-page
                                break;
                            }
                        }
                        if (isPageStart) {                                     /// NEW PAGE ///
                            width = qMax(width, cellX); // accumulate max width
                            cellX = iNameWidth;         // reset horizontal position

                            // This page's last measure and endTick
                            mPageEnd = mp;
                            pageTick = mp->tick() + mp->ticks() - startOffset;

                            // This page's cue_id, for scrolling by page
                            page_id = getCueID(mStartTick);
                            tableCues.append(page_id); // add page cue to VTT

                            // List of page cue_id/pageNumber pairs
                            qts.setString(&pageCues);
                            if (pageCues.isEmpty())
                                qts << SVG_CUE;
                            else
                                qts << SVG_COMMA;
                            qts << page_id << SVG_SEMICOLON
                                << QString("%1").arg(++idxPage + 1, 2, 10, QLatin1Char(SVG_ZERO));

                            // Initialize this page's collections
                            pageCols.append(0);

                            spl = new StrPtrList;
                            pageGridText.append(spl);

                            spv = new StrPtrVect(nStaves, 0);
                            pageStyles.append(spv);

                            spv = new StrPtrVect(nStaves, 0);
                            pageNames.append(spv);
                            for (Element* eAnn : s->annotations()) {
                                if (eAnn->type() == EType::INSTRUMENT_CHANGE)
                                    (*pageNames[idxPage])[eAnn->staffIdx()] = new QString(static_cast<InstrumentChange*>(eAnn)->xmlText());
                            }
                        }
                        // Reset indices for the new table or page
                        idxCol = 0;
                        idxBeat = 0;
                        idxBar = 0;
                    } // SVG
                } // if (new table or new page);

                // This grid column's cue_id
                gridTick  = crGrid->tick();
                gridTicks = crGrid->actualTicks();
                startTick = gridTick - startOffset;
                cue_id    = getCueID(startTick, startTick + gridTicks);
                tableCues.append(cue_id);

                if (!isPages || idxCol == grid.size()) {
                    pilv = new IntListVect(nStaves, 0);
                    pitches.append(pilv);

                    if (isPages) {
                        spv = new StrPtrVect(nStaves, 0);
                        dataCues.append(spv);

                        splv = new StrPtrListVect(nStaves, 0);
                        leds.append(splv);

                        if (twoVoices.size() > 0) {
                            spv = new StrPtrVect(nStaves, 0);
                            dataCues2.append(spv);
                            splv = new StrPtrListVect(nStaves, 0);
                            leds2.append(splv);
                        }
                    }
                    spv = new StrPtrVect(nStaves, 0);
                    if (twoVoices.size() > 0)
                        spv2 = new StrPtrVect(nStaves, 0);
                }
                else {
                    spv = grid[idxCol];
                    if (twoVoices.size() > 0)
                        spv2 = grid2[idxCol];
                }

                // Rows
                // Iterate over all the staves and collect stuff
                for (int r = 0; r < nStaves; r++) {
                    const int track = r * VOICES;
                    const bool has2 = twoVoices.indexOf(r) >= 0; // does this staff have 2 voices?

                    if (!isPages || isPageStart)
                        isStaffVisible[r] = m->system()->staff(r)->show();

                    // Set instrument name for this row once per table
                    //                 and if (isPages) once per page
                    if (r != idxGrid
                    && ((iNames[r] == 0 && isStaffVisible[r]) || isPageStart))   /// INSTRUMENT NAMES ///
                    {
                        if (isPageStart && isStaffVisible[r]) {
                            // Empty staff within a page is "invisible"
                            for (mp = m; mp; mp = mp->nextMeasureMM()) {
                                if (!mp->isOnlyRests(track))
                                    break;
                                if (mp == mPageEnd) { // empty staff for this page
                                    isStaffVisible[r] = false;
                                    break;
                                }
                            }
                        }
                        if (iNames[r] == 0 || isPageStart) {
                            const QString style = QString("inst%1").arg(isStaffVisible[r] ? NO : LO);
                            if (iNames[r] == 0) {
                                iNames[r] = new QString;
                                pqs = iNames[r];
                                qts.setString(pqs);
                                qts << SVG_CLASS << style << SVG_QUOTE;
                            }
                            if (isPageStart) {
                                (*pageStyles[idxPage])[r] = new QString(style);

                                if ((*pageNames[idxPage])[r] == 0) {
                                    if (idxPage == 0)
                                        (*pageNames[idxPage])[r] = new QString(score->staff(r)->part()->longName(startOffset));
                                    else
                                        (*pageNames[idxPage])[r] = (*pageNames[idxPage - 1])[r];
                                }
                            }
                        }
                    }

                    // Small staves are Grid, Chords, or Lyrics (all have text)
                    isLED = !score->staff(r)->small();

                    // The ChordRest for this staff, using only Voice #1
                    crData  = s->cr(track);
                    isChord = (crData != 0 && crData->isChord() && crData->visible());
                    if (isChord)
                        note = static_cast<Chord*>(crData)->notes()[0];

                    // Lyrics can have two voices in one staff, rest == empty
                    if (has2) {
                        cr2  = s->cr(track + 1);
                        isChord2 = (cr2 != 0 && cr2->isChord() && cr2->visible());
                        if (isChord2) {
                            note2 = static_cast<Chord*>(cr2)->notes()[0];
                            if (note2->tieBack() != 0)
                                isChord2 = false;  // just extends the duration of an existing chord
                        }
                    }
                    else
                        isChord2 = false;

                    // isChord and isChord2 have different rules:
                    //  - isChord2 is lyrics only, rest == empty
                    //  - isChord == false == not empty == rest in voice 1 LED
                    if (!isStaffVisible[r]
                    || (!isChord2 && (crData == 0
                                  || (isChord && note->tieBack() != 0)
                                  || (!isLED && !isChord))))
                    {                                                          /// EMPTY CELL ///
                        if (!isHTML && (isPages || isStaffVisible[r])) {
                            cellY += cellHeight; // for the next row
                        }
                        if (isPages) {
                            if ((*pitches[idxCol])[r] != 0)
                                (*pitches[idxCol])[r]->append(MIDI_EMPTY);
                            if ((*leds[idxCol])[r] != 0) {
                                if (isLED)
                                    pqs = new QString(SVG_HASH);
                                else
                                    pqs = new QString;
                                (*leds[idxCol])[r]->append(pqs);
                            }
                            if (has2 && (*leds2[idxCol])[r] != 0) {
                                pqs = new QString;
                                (*leds2[idxCol])[r]->append(pqs);
                            }
                        }
                        continue; // empty cell complete, on to the next cell
                    }

                    if (isLED || isChord) {
                        pqs = (*spv)[r];
                        if (!isPages || pqs == 0) {
                            pqs = new QString;
                            (*spv)[r] = pqs;
                        }
                        qts.setString(pqs);
                    }
                    if (isChord2) {
                        pqs2 = (*spv2)[r];
                        if (!isPages || pqs2 == 0) {
                            pqs2 = new QString;
                            (*spv2)[r] = pqs2;
                        }
                        qts2.setString(pqs2);
                    }

                    if (r == idxGrid) {                                        /// GRID CELL ///
                        // Grid staff cells are simple
                        if (isHTML)
                            qts << HTML_TH_BEGIN << SVG_GT
                                << crGrid->lyrics()[0]->plainText()
                                << HTML_TH_END;
                        else {
                            // Required if the idxGrid is not the first staff (which may not be plausible anyway...)
                            cue_id = getCueID(startTick, startTick + gridTicks);

                            QString ref = STAFF_GRID;
                            if (isPages && idxPage > 0)
                                ref += LO;
                            else
                                ref += NO;

                            // Grid <use> element
                            if (!isPages || idxCol == gridUse.size()) {
                                qts << SVG_USE    << SVG_SPACE
                                    << formatInt(SVG_X, cellX, maxDigits, true)
                                    << formatInt(SVG_Y, cellY, maxDigits, true)
                                    << XLINK_HREF << ref    << SVG_QUOTE
                                    << SVG_CUE    << cue_id;

                                if (!isPages)
                                    qts << SVG_QUOTE << SVG_ELEMENT_END << endl;
                                else {
                                    gridUse.append(pqs);
                                    pqs = new QString;  // For grid <text>, next
                                    qts.setString(pqs);
                                }
                            }
                            else { // gridUse[idxCol] exists, append cue_id
                                qts.setString(gridUse[idxCol]);
                                qts << SVG_COMMA << cue_id;
                            }

                            // Grid <text> element
                            const QString lyrics = crGrid->lyrics()[0]->plainText();

                            if (!isPages || idxCol == gridText.size()) {
                                qts << SVG_TEXT_BEGIN
                                    << formatInt(SVG_X, cellX + (cellWidth  / 2), maxDigits, true)
                                    << formatInt(SVG_Y, cellY + (cellHeight / 2), maxDigits, true)
                                    << SVG_CLASS << CLASS_GRID << NO << SVG_QUOTE // grid <text> LO = invisible = >empty content<
                                    << SVG_CUE   << cue_id;

                                if (!isPages)
                                    qts << SVG_QUOTE << SVG_GT << lyrics
                                        << SVG_TEXT_END << endl;
                                else
                                    gridText.append(pqs);
                            }
                            else { // gridText[r] exists (isPages == true)
                                qts.setString(gridText[idxCol]);
                                qts << SVG_COMMA << cue_id; // Highlight cue
                            }

                            // Rehearsal mark cue, for the markers ruler
                            for (Element* eAnn : s->annotations()) {
                                if (eAnn->type() == EType::REHEARSAL_MARK) {
                                    tableCues.append(getAnnCueID(score, eAnn, EType::REHEARSAL_MARK));
                                    break;
                                }
                            }

                            if (isPages) {
                                pageCols[idxPage]++;
                                pqs = new QString(lyrics);
                                pageGridText[idxPage]->append(pqs);
                            }

                            // Tempo change?
                            if (prevTempo != tempoMap->tempo(gridTick)) {
                                qts.setString(&tempoCues);
                                qts.setRealNumberNotation(QTextStream::FixedNotation);
                                qts.setRealNumberPrecision(BPM_PRECISION);

                                // Is this the first tempo change?
                                if (tempoCues.isEmpty()) {
                                    qts << SVG_CUE       << CUE_ID_ZERO
                                        << SVG_SEMICOLON << initialBPM * 1.0 << TEXT_BPM;
                                    prevTempoPage = 1;
                                }

                                // Any missing page cues
                                while (idxPage > prevTempoPage)
                                    qts << SVG_COMMA     << pageIDs[prevTempoPage++]
                                        << SVG_SEMICOLON << prevTempo * 60.0 << TEXT_BPM;

                                // The tempo change's cue
                                prevTempo  = tempoMap->tempo(gridTick);
                                qts << SVG_COMMA     << getCueID(startTick)
                                    << SVG_SEMICOLON << prevTempo * 60.0 << TEXT_BPM;
                            }

                            // Grid <line> for whole beats e,g, 1/4 note in 4/4 time
                            const int x     = cellX * (isPages && idxPage > 0 ? -1 : 1);
                            const int tick  = startTick - mStartTick;
                            const int denom = mTicks
                                            / score->staff(r)->timeSig(startTick)->sig().denominator();
                            if (tick && !(tick % denom)) {
                                if (!isPages || idxBeat == beatLines.size()) {
                                    // Initial x-coord for line: if it's not
                                    // page 1, it's invisible via negative x.
                                    pqs = new QString;
                                    qts.setString(pqs);
                                    qts << SVG_LINE
                                           << SVG_X1    << x          << SVG_QUOTE
                                           << SVG_Y1    << cellHeight
                                                         + gridMargin << SVG_QUOTE
                                           << SVG_X2    << x          << SVG_QUOTE
                                           << SVG_Y2    << height
                                                         - cellHeight
                                                         - gridMargin << SVG_QUOTE
                                           << SVG_CLASS << "beatLine" << SVG_QUOTE;
                                    if (!isPages)
                                        qts << SVG_ELEMENT_END        << endl;

                                    beatLines.append(pqs);
                                }
                                if (isPages) {
                                    if (pageBeats.size() == idxBeat) {
                                        pil = new IntList;
                                        pageBeats.append(pil);
                                    }
                                    while (pageBeats[idxBeat]->size() < idxPage)
                                        pageBeats[idxBeat]->append(x); // missing pages = invisible gridline
                                    pageBeats[idxBeat]->append(cellX);
                                    idxBeat++;
                                }
                            }
                        }
                    } // if(idxGrid)
                    else {                                                     /// DATA CELL ///
                        // Data cells are not as simple
                        if (isChord)
                            dataTicks = note->playTicks(); // handles tied notes, secondary tied notes excluded above (note->tieBack() != 0)
                        else if (!has2)
                            dataTicks = crData->actualTicks();
                        else
                            dataTicks = gridTicks; // makes things simpler in code below

                        if (isChord2)
                            dataTicks2 = note->playTicks();
                        else
                            dataTicks2 = gridTicks;

                        if (dataTicks < gridTicks
                        || (isChord2 && dataTicks2 < gridTicks))
                            return false; // Dies ist verboten

                        const int colSpan  = dataTicks  / gridTicks;
                        const int colSpan2 = dataTicks2 / gridTicks;

                        cue_id  = getCueID(startTick, startTick + dataTicks);
                        if (isChord2)
                            cue_id2 = getCueID(startTick, startTick + dataTicks2);

                        if (isHTML) {
                            // Start the <td> element
                            qts << HTML_TD_BEGIN;
                            if (colSpan > 1)
                                // Usually cells only span one column but not always.
                                // Add cue_id and colspan attributes to this <td>.
                                qts << SVG_CUE   << cue_id
                                    << SVG_QUOTE << HTML_COLSPAN << colSpan << SVG_QUOTE;
                            qts << SVG_GT;

                            // <td>content only if it's a note in this staff
                            if (isChord) {
                                const TDuration dur = crData->durationType();

                                qts << XML_ENTITY_BEGIN
                                    << QString::number(durationUnicode[int(dur.type())], 16).toUpper()
                                    << XML_ENTITY_END;

                                for (int j = 0; j < dur.dots(); j++)
                                    qts << UNICODE_DOT;
                            }
                            // Complete the <td>...</td>
                            qts << HTML_TD_END;
                        }
                        else { // SVG:
                            QString cellValue;
                            QString cellValue2;

                            if (isLED)
                                cellValue = QString("%1%2%3")
                                           .arg(LED)
                                           .arg(colSpan > 1 ? QString::number(colSpan) : "")
                                           .arg(isChord     ? NO                       : LO);
                            else if (r == idxChords) {
                                Harmony* pHarm = getHarmony(s);
                                if (pHarm != 0) // this prevents crashes for misaligned scores
                                    cellValue = pHarm->rootName();
                            }
                            else { // Lyrics
                                if (isChord)
                                    cellValue = crData->lyrics()[0]->plainText();
                                if (isChord2)
                                    cellValue2 = cr2->lyrics()[0]->plainText();
                            }

                            int x  = cellX + (cellWidth / 2);
                            int y;
                            const QString lyric = "lyric";
                            const QString chord = "chord";

                            if (pqs->isEmpty()) {
                                if (isLED) { // LED staff, notes not text
                                    qts << SVG_USE << SVG_SPACE
                                        << formatInt(SVG_X, cellX, maxDigits, true)
                                        << SVG_Y << SVG_PERCENT // for multi-pitch
                                        << XLINK_HREF;

                                    if (!isPages || idxPage == 0)
                                        qts << cellValue;

                                    qts << SVG_QUOTE;
                                }
                                else if (isChord) {
                                    y = baseline - (has2 ? 15 : 1);
                                    qts << SVG_TEXT_BEGIN
                                        << formatInt(SVG_X, x, maxDigits, true)
                                        << formatInt(SVG_Y, y, maxDigits, true)
                                        << SVG_CLASS << (r == idxChords ? chord : lyric)
                                        << NO << SVG_QUOTE;
                                }
                            }
                            if (isChord2 && pqs2->isEmpty()) {
                                y = baseline + 5;
                                qts2 << SVG_TEXT_BEGIN
                                    << formatInt(SVG_X, x, maxDigits, true)
                                    << formatInt(SVG_Y, y, maxDigits, true)
                                    << SVG_CLASS << lyric << NO << SVG_QUOTE;
                            }

                            if (!isPages) { // fully-formed data-cue="cue_id"
                                if (isChord)
                                    qts << SVG_CUE << cue_id << SVG_QUOTE;
                                if (isChord2)
                                    qts2 << SVG_CUE << cue_id2 << SVG_QUOTE;
                            }

                            // This ChordRest's pitch
                            if (isLED) {
                                pil = (*pitches[idxCol])[r];  // this cell's collection of pitches
                                if (pil == 0) {
                                    pil = new IntList;
                                    (*pitches[idxCol])[r] = pil;
                                }

                                while (pil->size() < idxPage) // fill in previous pages' empty values
                                    pil->append(MIDI_EMPTY);

                                if (pil->size() == idxPage)   // this ChordRest's pitch value
                                    pil->append(isChord ? note->pitch() : MIDI_REST);
                            }

                            if (isPages) {
                                // Highlight cues: Rests don't have them
                                if (isChord) {
                                    // This cell's data-cue string
                                    pqs = (*dataCues[idxCol])[r];
                                    if (pqs == 0) {
                                        pqs = new QString;
                                        (*dataCues[idxCol])[r] = pqs;
                                    }
                                    qts.setString(pqs);

                                    // Add this cue to the comma-separated string
                                    if (pqs->isEmpty())
                                        qts << SVG_CUE;
                                    else
                                        qts << SVG_COMMA;
                                    qts << cue_id;
                                }

                                // LED or Chord or Lyrics value for page cues
                                if (isLED || isChord) {
                                    spl = (*leds[idxCol])[r];
                                    if (spl == 0) {
                                        spl = new StrPtrList;
                                        (*leds[idxCol])[r] = spl;
                                    }
                                    while (spl->size() <= idxPage) {
                                        pqs = new QString();
                                        if (isLED)
                                            pqs->append(SVG_HASH);

                                        if (spl->size() == idxPage)
                                            pqs->append(cellValue);

                                        spl->append(pqs);
                                    }
                                }

                                if (isChord2) {
                                    // This cell's data-cue string
                                    pqs2 = (*dataCues2[idxCol])[r];
                                    if (pqs2 == 0) {
                                        pqs2 = new QString;
                                        (*dataCues2[idxCol])[r] = pqs2;
                                    }
                                    qts2.setString(pqs2);

                                    // Add this cue to the comma-separated string
                                    if (pqs2->isEmpty())
                                        qts2 << SVG_CUE;
                                    else
                                        qts2 << SVG_COMMA;
                                    qts2 << cue_id;

                                    // This cell's lyrics text
                                    spl = (*leds2[idxCol])[r];
                                    if (spl == 0) {
                                        spl = new StrPtrList;
                                        (*leds2[idxCol])[r] = spl;
                                    }
                                    while (spl->size() <= idxPage) {
                                        pqs2 = new QString();
                                        if (spl->size() == idxPage)
                                            pqs2->append(cellValue2);

                                        spl->append(pqs2);
                                    }
                                }
                            }

                            if (colSpan > 1) // additional, non-grid cue_id
                                tableCues.append(cue_id);
                            if (colSpan2 > 1)
                                tableCues.append(cue_id2);

                            if (!isPages) {    // close the element(s)
                                if (isLED)
                                    qts << SVG_ELEMENT_END << endl;
                                else if (isChord)
                                    qts << SVG_GT << crData->lyrics()[0]->plainText()
                                        << SVG_TEXT_END;
                                else if (isChord2)
                                    qts2 << SVG_GT << cr2->lyrics()[0]->plainText()
                                         << SVG_TEXT_END;
                            }
                        } // else: SVG
                    } // else: r != idxGrid

                    cellY += cellHeight; // Move to the next row/staff

                } // for (r < nStaves)

                // Append new vectors (columns) to the list (grid)
                if (!isPages || idxCol == grid.size()) {
                    grid.append(spv);
                    if (twoVoices.size() > 0)
                        grid2.append(spv2);
                }

                if (isPages)
                    idxCol++;

                height = cellY + cellHeight; // Extra row for title/buttons
                cellY = 0;
                cellX += cellWidth;

            } // if (crGrid->isChord()) - it's a live grid column

            if (isPageStart) {
                isPageStart = false;     // page start is always on measure start
                pageIDs.append(page_id); // this page's cue_id
            }

        } // for (segments within this measure)

        if (m->repeatEnd()) {                                                  /// TABLE END ///
            // This measure is the end of the pattern/table, deal with it:
            if (isHTML) {
                // Stream the <col> elements, instrument names column first
                tableStream << HTML_COL_BEGIN << SVG_GT  << endl;
                for (QStringList::iterator i  = tableCues.begin();
                                           i != tableCues.end();
                                         ++i)
                    tableStream << HTML_COL_BEGIN << SVG_CUE << *i
                                << SVG_QUOTE      << SVG_GT  << endl;

                // Stream the <tr>, <th>, and <td> elements: by row r, by col c
                for (int r = 0; r < nStaves; r++) {
                    bool isRowStarted = false;
                    for (int c = 0; c < grid.size(); c++) {
                        if (grid[c]->value(r) != 0) {
                            if (!isRowStarted) {
                                // Start the row and add the instrument name cell
                                tableStream << HTML_TR_BEGIN << SVG_GT << SVG_SPACE
                                            << HTML_TD_BEGIN << SVG_CLASS;
                                if (r != idxGrid)
                                    tableStream << CLASS_INSTRUMENT << SVG_QUOTE << SVG_GT
                                                << score->staff(r)->part()->longName(startOffset);
                                else
                                    tableStream << CLASS_TITLE      << SVG_QUOTE << SVG_GT
                                                << tableTitle;

                                tableStream << HTML_TD_END << SVG_SPACE;

                                isRowStarted = true;
                            }
                            tableStream << *(grid[c]->value(r)) << SVG_SPACE;
                        }
                    }
                    if(isRowStarted)
                        tableStream << HTML_TR_END << endl;
                }

                // </table>\n
                tableStream << HTML_TABLE_END << endl << endl;

            } // if (isHtml)
            else { // SVG = One file per table
                const bool isOneTable = (m->no() == score->lastMeasure()->no() && startOffset == 0);
                const QString tblTtl  = (isOneTable ? "" : QString("%1%2").arg(SVG_DASH).arg(tableTitle));
                fnTable = QString("%1%2").arg(fnRoot).arg(tblTtl);
                tableFile.setFileName(QString("%1%2").arg(fnTable).arg(EXT_SVG));
                tableFile.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
                tableStream.setDevice(&tableFile);

                if (hasRulers)
                    height += RULER_HEIGHT; // Ruler adds to the total SVG height
                else
                    height -= cellHeight;   // No ruler == no title bar/playback buttons either

                if (isPages)
                    pageCues += SVG_QUOTE;  // This needs termination:

                // Stream the SVG header elements CSS, <svg>, <title>, <desc>:
                tableStream
                    << XML_STYLE_GRID // CSS Stylesheet
                    << SVG_BEGIN      // <svg>
                       << XML_NAMESPACE << XML_XLINK << SVG_4SPACES
                       << SVG_VIEW_BOX  << SVG_ZERO  << SVG_SPACE
                                        << SVG_ZERO  << SVG_SPACE
                                        << width     << SVG_SPACE
                                        << height    << SVG_QUOTE
                       << SVG_WIDTH     << width     << SVG_QUOTE
                       << SVG_HEIGHT    << height    << SVG_QUOTE
                                                           << endl << SVG_4SPACES
                       << SVG_PRESERVE_XYMIN_MEET          << endl << SVG_4SPACES
                       << SVG_POINTER_EVENTS << SVG_CURSOR << endl << SVG_4SPACES
                       << SVG_CLASS << SMAWS << SVG_QUOTE
                       << (hasRulers ? SVG_ONLOAD : "")
                       << (isPages   ? pageCues   : "")
                    << SVG_GT << endl
                    // <title>
                    << SVG_TITLE_BEGIN << score->title() << SVG_TITLE_END << endl
                    // <desc>
                    << SVG_DESC_BEGIN  << smawsDesc(score)    << SVG_DESC_END
                << endl;

                // Import the <defs>
                QFile qf;
                qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_GRID_DEFS));
                qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
                qts.setDevice(&qf);
                tableStream << endl << qts.readAll() << endl;

                // The background rects (pattern + gradient in one fill == not)
                tableStream << SVG_RECT
                            << SVG_X << SVG_QUOTE   << SVG_ZERO << SVG_QUOTE
                            << SVG_Y << SVG_QUOTE   << SVG_ZERO << SVG_QUOTE
                            << SVG_WIDTH            << width    << SVG_QUOTE
                            << SVG_HEIGHT           << height   << SVG_QUOTE
                            << SVG_FILL_URL         << "gradBg" << SVG_RPAREN_QUOTE
                            << SVG_ELEMENT_END      << endl
                            << SVG_RECT
                            << SVG_X << SVG_QUOTE   << SVG_ZERO << SVG_QUOTE
                            << SVG_Y << SVG_QUOTE   << SVG_ZERO << SVG_QUOTE
                            << SVG_WIDTH            << width    << SVG_QUOTE
                            << SVG_HEIGHT           << height   << SVG_QUOTE
                            << SVG_FILL_URL         << "pattBg" << SVG_RPAREN_QUOTE
                            << SVG_CLASS            << "background"    << SVG_QUOTE
                            << SVG_ELEMENT_END      << endl << endl;

                if (isPages) {
                    // This is just more convenient - now it's page count
                    idxPage++;

                    // Complete the barLines and barNums values
                    for (int b = 0; b < barLines.size(); b++) {
                        // barLines
                        bool hasPageCues = false;
                        if (pageBars[b]->size() < idxPage)     // missing pages
                            hasPageCues = true;
                        else {
                            const int x = (*pageBars[b])[0];   // initial x
                            for (int p = 1; p < idxPage; p++) {
                                if ((*pageBars[b])[p] != x) {  // diff x-coords
                                    hasPageCues = true;
                                    break;
                                }
                            }
                        }
                        qts.setString(barLines[b]);
                        if (hasPageCues) { // page cues required for barLines[b]
                            for (int p = 0; p < idxPage; p++) {
                                if (p > 0)
                                    qts << SVG_COMMA;
                                else
                                    qts << SVG_CUE;

                                qts << pageIDs[p] << SVG_SEMICOLON;

                                if (p >= pageBars[b]->size())
                                    pageBars[b]->append(-100);  // -1 isn't far enough off the canvas

                                qts << (*pageBars[b])[p];
                            }
                            qts << SVG_QUOTE;
                        }
                        qts << SVG_ELEMENT_END << endl;

                        // barNums
                        const int num = (*pageBarNums[b])[0]; // initial barNum
                        if (!hasPageCues) {
                            for (int p = 1; p < idxPage; p++) {
                                if ((*pageBarNums[b])[p] != num) {   // diff barNums
                                    hasPageCues = true;
                                    break;
                                }
                            }
                        }

                        qts.setString(barNums[b]);
                        if (hasPageCues) { // page cues required for barNums[b]
                            for (int p = 0; p < idxPage; p++) {
                                if (p > 0)
                                    qts << SVG_COMMA;
                                else
                                    qts << SVG_CUE;

                                qts << pageIDs[p] << SVG_SEMICOLON;

                                if (p < pageBarNums[b]->size() && (*pageBarNums[b])[p] > -100)
                                    qts << (*pageBarNums[b])[p];

                                qts << SVG_SEMICOLON << (*pageBars[b])[p] + 4;
                            }
                            qts << SVG_QUOTE;
                        }
                        qts << SVG_GT << num << SVG_TEXT_END << endl;
                    }
                }

                // Stream the barLines and barNums (if they exist)
                for (int b = 0; b < barLines.size(); b++)
                    tableStream << *barLines[b] << *barNums[b];
                tableStream << endl;

                // Stream the beatLines (beat and barline/barnum loops could be consolidated in a function, maybe?)
                for (int b = 0; b < beatLines.size(); b++) {
                    tableStream << *beatLines[b];
                    if (idxPage > 1) { // are there page cues for this beatline?
                        bool hasPageCues = false;
                        if (pageBeats[b]->size() < idxPage)     // missing pages
                            hasPageCues = true;
                        else {
                            const int x = (*pageBeats[b])[0];   // initial x
                            for (int p = 1; p < idxPage; p++) {
                                if ((*pageBeats[b])[p] != x) {  // diff x-coords
                                    hasPageCues = true;
                                    break;
                                }
                            }
                        }
                        if (hasPageCues) { // page cues required
                            tableStream << SVG_CUE;
                            for (int p = 0; p < idxPage; p++) {
                                if (p > 0)
                                    tableStream << SVG_COMMA;
                                tableStream << pageIDs[p] << SVG_SEMICOLON;
                                if (p < pageBeats[b]->size())
                                    tableStream << (*pageBeats[b])[p];
                                else
                                    tableStream << -100;  // -1 isn't far enough off the canvas
                            }
                            tableStream << SVG_QUOTE;
                        }
                        tableStream << SVG_ELEMENT_END << endl;
                    }
                }
                tableStream << endl;

                const int colCount = grid.size(); // max # of columns per page
                if (isPages) {
                    // Stream the grid staff separately
                    int minCols = colCount;       // min # of columns per page
                    for (int p = 0; p < idxPage; p++) {
                        if (pageCols[p] < minCols)
                            minCols = pageCols[p];
                    }
                    for (int g = 0; g < colCount; g++) {
                        tableStream << *gridUse[g];
                        if (g >= minCols){ // grayed-out <use>s by page
                            for (int p = 0; p < idxPage; p++) {
                                tableStream << SVG_COMMA     << pageIDs[p]
                                            << SVG_SEMICOLON << SVG_HASH
                                            << CLASS_GRID;
                                if (g < pageCols[p])
                                    tableStream << NO;
                                else
                                    tableStream << LO;
                            }
                        }
                        tableStream << SVG_QUOTE
                                    << SVG_ELEMENT_END  << endl
                                    << *gridText[g];

                        bool hasPageCues = (g >= minCols);
                        if (!hasPageCues) {
                            pqs = (*pageGridText[0])[g];
                            for (int p = 0; p < idxPage; p++) {
                                if (*(*pageGridText[p])[g] != *pqs) {
                                    hasPageCues = true;
                                    break;
                                }
                            }
                        }
                        if (hasPageCues) {
                            for (int p = 0; p < idxPage; p++) {
                                tableStream << SVG_COMMA << pageIDs[p] << SVG_SEMICOLON;
                                if (g < pageCols[p])
                                    tableStream << *(*pageGridText[p])[g];
                            }
                        }

                        tableStream << SVG_QUOTE << SVG_GT;
                        if (g < pageCols[0])
                            tableStream << *(*pageGridText[0])[g];
                        tableStream << SVG_TEXT_END  << endl;
                    }
                    tableStream << endl;
                }

                // Handle variable pitch in the rows that have it
                RealVect intervals(nStaves, 0); // pitch vertical increment

                // Convert the pitch values to ordinal values within a row
                for (int r = 0; r < nStaves; r++) {
                    pil = new IntList;
                    for (int c = 0; c < colCount; c++) {
                        if ((*pitches[c])[r] != 0) {
                            pil->append(*(*pitches[c])[r]);
                        }
                    }
                    if (pil->size() == 0)
                        continue;
                    pil = new IntList(pil->toSet().toList());
                    std::sort(pil->begin(), pil->end());
                    while (pil->size() > 0 && pil->first() < 0)
                        pil->pop_front();  // remove rests/empties from the set
                    if (pil->size() > 1) {
                        intervals[r] = verticalSpace / (pil->size() - 1);
                        std::reverse(pil->begin(), pil->end()); // higher pitch = lower y-offset
                    }
                    pitchSet[r] = pil;
                }

                // Stream the cells by row by column (by page).
                // Rows are streamed left-to-right.
                QString name;
                QString id;

                cellY = 0;
                for (int r = 0; r < nStaves; r++) {
                    // Stream the instrument names - similar loop to bar/beatlines
                    if (r != idxGrid && iNames[r] != 0) {
                        // Display name (text element's innerHTML)
                        name = score->staff(r)->part()->longName(startOffset);

                        // This id value works best with only alphanumeric chars.
                        id = name;
                        id.remove(QRegExp("[^a-zA-Z\\d]"));

                        isLED = !score->staff(r)->small();

                        // Each staff (row) is wrapped in a group
                        tableStream << SVG_GROUP_BEGIN
                                       << SVG_ID        << id  << SVG_QUOTE
                                       << SVG_CLASS     << "staff"
                                       << (!isLED && r != idxChords ? " lyrics" : "") << SVG_QUOTE
                                       << SVG_INAME     << score->staff(r)->part()->shortName(startOffset) << SVG_QUOTE
                                       << SVG_TRANSFORM << SVG_TRANSLATE << SVG_ZERO
                                       << SVG_SPACE     << cellY << SVG_RPAREN_QUOTE
                                    << SVG_GT << endl;

                        if (r != idxChords) {
                            // Import the row header controls (solo, gain, pan), but not for idxChord
                            qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_GRID_INST));
                            qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
                            qts.setDevice(&qf);
                            tableStream << qts.readAll().replace("%0", evtPrefix)
                                                        .replace("%1", id)
                                                        .replace("%2", *iNames[r]);
                        }
                        else // the one and only chords row, no controls, but it has a name/label
                            tableStream << SVG_TEXT_BEGIN
                                        << formatInt(SVG_X, nameLeft, maxDigits, true)
                                        << formatInt(SVG_Y, baseline, maxDigits, true)
                                        << SVG_ID << id << SVG_QUOTE
                                        << *iNames[r];

                        if (isPages) {
                            bool changesName  = false;
                            pqs = (*pageNames[0])[r];
                            for (int p = 1; p < idxPage; p++) {
                                 if (*(*pageNames[p])[r] != *pqs) {
                                    changesName = true;
                                    break;
                                }
                            }
                            bool changesStyle = false;
                            pqs = (*pageStyles[0])[r];
                            for (int p = 1; p < idxPage; p++) {
                                if (*(*pageStyles[p])[r] != *pqs) {
                                    changesStyle = true;
                                    break;
                                }
                            }
                            if (changesName || changesStyle) { // page cues required
                                tableStream << SVG_CUE;
                                for (int p = 0; p < idxPage; p++) {
                                    if (p > 0)
                                        tableStream << SVG_COMMA;
                                    tableStream << pageIDs[p] << SVG_SEMICOLON;
                                    if (changesName)
                                        tableStream << *(*pageNames[p])[r];
                                    if (changesStyle)
                                        tableStream << SVG_SEMICOLON
                                                    << *(*pageStyles[p])[r];
                                }
                                tableStream << SVG_QUOTE;
                            }
                        }
                        // Stream the >content</text>
                        tableStream << SVG_GT << name << SVG_TEXT_END << endl;

                        // Spacer below staff = instLine below staff in SVG
                        // Spacer must be a down spacer in the first measure
                        if (m1->mstaff(r)->_vspacerDown != 0) {
                            const int y = cellHeight; // * (r + 1);
                            const int xMargin = 3;
                            if (isPages)
                                tableStream << SVG_4SPACES; // indentation inside group

                            tableStream << SVG_LINE
                                        << SVG_X1      << xMargin         << SVG_QUOTE
                                        << SVG_Y1      << y               << SVG_QUOTE
                                        << SVG_X2      << width - xMargin << SVG_QUOTE
                                        << SVG_Y2      << y               << SVG_QUOTE
                                        << SVG_CLASS   << "instLine"      << SVG_QUOTE
                                        << SVG_ELEMENT_END;
                        }
                        tableStream << endl;
                    }

                    // The data cells (and grid cells if isPage == false)
                    if (isPages && r != idxGrid) {
                        tableStream << SVG_4SPACES  << SVG_GROUP_BEGIN
                                       << SVG_ID    << id          << SVG_QUOTE
                                       << SVG_CLASS << CLASS_NOTES << SVG_QUOTE
                                    << SVG_GT << endl;
                    }

                    const bool hasPitches = (pitchSet[r] != 0
                                          && pitchSet[r]->size() > 1);

                    for (int c = 0; c < grid.size(); c++) {
                        if ((*grid[c])[r] == 0 || (isPages && r == idxGrid))
                            continue;

                        int  pitch0;
                        qreal y = 0;

                        pqs = (*grid[c])[r];
                        pil = (*pitches[c])[r];
                        if (isLED) {
                            if (hasPitches) {
                                pitch0 = (*pil)[0];
                                const int idx = pitchSet[r]->indexOf(pitch0);
                                y = (idx >= 0 ? intervals[r] * idx : restOffset);
                                pqs->replace(LED, MINI);
                            }
                            pqs->replace(QString("%1%2")    .arg(SVG_Y).arg(SVG_PERCENT),
                                         QString("%1%2%3%4").arg(SVG_Y).arg(SVG_QUOTE)
                                                            .arg(y)    .arg(SVG_QUOTE));
                        }

                        if (isPages && r != idxGrid)
                            tableStream << SVG_4SPACES; // indentation inside group

                        tableStream << *pqs;

                        if (isPages) {                        // Â¿Page Cues?
                            bool changesPitch = false;
                            bool changesValue = false;
                            bool hasCues      = ((*dataCues[c])[r] != 0);
                            if (hasCues)
                                tableStream << *(*dataCues[c])[r];

                            spl  = (*leds[c])[r];
                            if (spl != 0)
                                pqs  = (*spl)[0];
                            for (int p = 0; p < idxPage; p++) {
                                if (hasPitches && !changesPitch && (*pil)[p] != pitch0)
                                    changesPitch = true;

                                if (!changesValue
                                 && spl != 0
                                 && ((*spl)[p]->size() == 1 || (*spl)[p] != *pqs))
                                    changesValue = true;

                                if (changesValue && (!hasPitches || changesPitch))
                                    break;
                            }

                            bool hasPageCues = (changesPitch || changesValue);
                            if (hasPageCues) { // Page Cues
                                for (int p = 0; p < idxPage; p++) {
                                    if (p == 0 && !hasCues)
                                        tableStream << SVG_CUE;
                                    else
                                        tableStream << SVG_COMMA;
                                    tableStream << pageIDs[p] << SVG_SEMICOLON;

                                    if (changesValue) {
                                        if (spl->size() > p) {
                                            if (hasPitches)
                                                (*spl)[p]->replace(LED, MINI);
                                            tableStream << *(*spl)[p];
                                        }
                                        else if (isLED)
                                            tableStream << SVG_HASH;
                                    }

                                    if (changesPitch) {
                                        tableStream << SVG_SEMICOLON;
                                        if (pil->size() > p) {
                                            const int pitch = (*pil)[p];
                                            if (pitch == MIDI_EMPTY)
                                                tableStream << -100;  // effectively invisible
                                            else if (pitch == MIDI_REST)
                                                tableStream << (hasPitches ? restOffset : 0);
                                            else
                                                tableStream << (intervals[r] * pitchSet[r]->indexOf(pitch));
                                        }
                                        else
                                            tableStream << -100; // effectively invisible
                                    }
                                }
                            }
                            if (hasCues || hasPageCues)
                                tableStream << SVG_QUOTE;

                            if (isLED)
                                tableStream << SVG_ELEMENT_END << endl;
                            else {  // Chords and Lyrics are text, with innerHTML and </text>
                                QString innerHTML;
                                if (spl != 0)
                                    innerHTML = *pqs;
                                tableStream << SVG_GT << innerHTML << SVG_TEXT_END << endl;
                            }
                        } // if isPages, Voice #1

                        // Lyrics staff can have 2 voices
                        if (twoVoices.indexOf(r) < 0 || (*grid2[c])[r] == 0)
                            continue;

                        // Voice #2
                        if (isPages && r != idxGrid)
                            tableStream << SVG_4SPACES; // indentation inside group

                        tableStream << *(*grid2[c])[r]; // text element begin

                        if (isPages) {                        // page      cues
                            if ((*dataCues2[c])[r] != 0) {    // highlight cues
                                tableStream << *(*dataCues2[c])[r];

                                spl = (*leds2[c])[r];
                                if (spl != 0) {
                                    for (int p = 0; p < idxPage; p++) {
                                        tableStream << SVG_COMMA;
                                        tableStream << pageIDs[p] << SVG_SEMICOLON;
                                        tableStream << (spl->size() > p ? *(*spl)[p] : "");
                                    }
                                }
                            }
                            tableStream << SVG_QUOTE  << SVG_GT
                                        << ((*spl)[0] != 0 ? *(*spl)[0] : "")
                                        << SVG_TEXT_END << endl;
                        }
                    } // for each column

                    if (r != idxGrid) {
                        if (isPages)
                            tableStream << SVG_4SPACES << SVG_GROUP_END << endl
                                                       << SVG_GROUP_END << endl;

                        tableStream << endl;
                    }
                    cellY += cellHeight;

                } // for each row

                // If rulers are embedded in the SVG file
                if (hasRulers) {
                    // Top of the rulers
                    height -= RULER_HEIGHT;

                    // Line dividing main panel from rulers/counters
                    tableStream << SVG_LINE  << SVG_CLASS << "divider" << SVG_QUOTE
                                   << SVG_X1 << SVG_ZERO  << SVG_QUOTE
                                   << SVG_Y1 << height    << SVG_QUOTE
                                   << SVG_X2 << width     << SVG_QUOTE
                                   << SVG_Y2 << height    << SVG_QUOTE
                                << SVG_ELEMENT_END        << endl;

                    // Rulers: Bars and RehearsalMarks
                    QString bars;
                    QTextStream barStream(&bars);
                    tableStream << SVG_GROUP_BEGIN  << SVG_ID        << "markers"
                                << SVG_QUOTE        << SVG_TRANSFORM << SVG_TRANSLATE
                                << SVG_ZERO         << SVG_SPACE     << height
                                << SVG_RPAREN_QUOTE << SVG_GT        << endl;

                    IntSet setVTT;
                    streamRulers(score, qfi, &barStream, &tableStream, &setVTT, width, evtPrefix, SVG_4SPACES);

                    tableStream << SVG_GROUP_END    << endl          << endl
                                << SVG_GROUP_BEGIN  << SVG_ID        << "bars"
                                << SVG_QUOTE        << SVG_TRANSFORM << SVG_TRANSLATE
                                << SVG_ZERO         << SVG_SPACE     << height + qFloor(RULER_HEIGHT / 2)
                                << SVG_RPAREN_QUOTE << SVG_GT        << endl
                                << bars             << SVG_GROUP_END << endl << endl;
                }

                // Terminate this string if it exists
                if (!tempoCues.isEmpty()) {
                    qts.setRealNumberNotation(QTextStream::FixedNotation);
                    qts.setRealNumberPrecision(BPM_PRECISION);
                    while (idxPage > prevTempoPage) // Complete any missing page cues
                        qts << SVG_COMMA << pageIDs[prevTempoPage++] << SVG_SEMICOLON << prevTempo * 60.0 << TEXT_BPM;

                    tempoCues.append(SVG_QUOTE);
                }

                // Import and vertically position the buttons/title.
                // This file includes the reference to the external javascript,
                // as well as the audio and vtt files.
                // This text file is very small, QString.replace() = no problem
                // No rulers            == only page buttons
                // Rulers, but no pages == only play buttons
                // Rulers + pages       == both page and play buttons
                qf.setFileName(QString("%1/%2").arg(qfi->path())
                                               .arg(!hasRulers ? FILE_GRID_TEMPO
                                                               : (isPages ? FILE_GRID_BOTH
                                                                          : FILE_GRID_PLAY)));

                qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
                qts.setDevice(&qf);

                // The %N numbering is the same across all three files
                tableStream << qts.readAll().replace("%0", evtPrefix)
                                            .replace("%1", QString::number(height - 47))
                                            .replace("%2", QString::number(height - 10.5))
                                            .replace("%3", tableTitle)
                                            .replace("%4", qfi->completeBaseName())
                                            .replace("%5", QString::number(width - 8))
                                            .replace("%6", tempoCues)
                                            .replace("%7", QString::number(initialBPM, 'f', BPM_PRECISION));
                // </svg>
                tableStream << SVG_END;

                // Write and close the SVG file for this table
                tableStream.flush();
                tableFile.close();

                // The pseudo-empty cue that is the end-of-audio has this Cue ID:
                tableCues.append(getCueID(m->last()->tick()));

                // Write the VTT file for this table
                if (!saveVTT(score, fnTable, tableCues))
                    return false;

                // Reset this for the next time around
                cellX = iNameWidth;
            }

            // Reset for next table
            width     = 0;
            height    = 0;
            cellY     = 0;
            gridTicks = 0;
            grid.clear();
            gridUse.clear();
            gridText.clear();
            barNums.clear();
            barLines.clear();
            beatLines.clear();
            tableCues.clear();
            pitches.clear();
            tempoCues.clear();
            for (int r = 0; r < nStaves; r++) {
                iNames[r]   = 0;
                pitchSet[r] = 0;
                if (isPages)
                    isStaffVisible[r] = 0;
            }
            if (isPages) {
                isPages  = false;
                pageTick =  0;
                idxPage  = -1;
                leds.clear();
                pageIDs.clear();
                dataCues.clear();
                pageCues.clear();
                pageCols.clear();
                pageBars.clear();
                pageBeats.clear();
                pageNames.clear();
                pageStyles.clear();
                pageBarNums.clear();
                pageGridText.clear();
            }
        } // if(Repeat::END)
        else {
            const int n = m->no() + 2; // this is the bar number for the next bar
            const int x = (isPages && idxPage > 0 ? -100 : cellX - barRound);
            if (!isHTML
            && (!isPages || (idxBar == barLines.size() && m != mPageEnd))) {
                // End-of-Bar lines for the all but the last bar of the pattern.
                // <rect> because <line>s are funky with gradients.
                pqs = new QString;
                qts.setString(pqs);
                qts << SVG_RECT
                       << SVG_X << SVG_QUOTE << x                  << SVG_QUOTE
                       << SVG_Y << SVG_QUOTE << barMargin - 2      << SVG_QUOTE
                       << SVG_WIDTH          << barWidth           << SVG_QUOTE
                       << SVG_HEIGHT         << height - cellHeight
                                              - barMargin - 2      << SVG_QUOTE
                       << SVG_RX             << barRound           << SVG_QUOTE
                       << SVG_RY             << barRound           << SVG_QUOTE
                       << SVG_CLASS          << "barLine"          << SVG_QUOTE;
                if (!isPages)
                    qts << SVG_ELEMENT_END   << endl;

                barLines.append(pqs);

                // Bar numbers for the bar lines
                pqs = new QString;
                qts.setString(pqs);
                qts << SVG_TEXT_BEGIN
                       << SVG_X << SVG_QUOTE << x + 4              << SVG_QUOTE
                       << SVG_Y << SVG_QUOTE << barMargin          << SVG_QUOTE
                       << SVG_CLASS          << "barNumber"        << SVG_QUOTE;
                if (!isPages)
                    qts << SVG_GT << n << SVG_TEXT_END   << endl;

                barNums.append(pqs);
            }
            if (isPages) {
                if (pageBars.size() == idxBar) {
                    pil = new IntList;
                    pageBars.append(pil);
                    pil = new IntList;
                    pageBarNums.append(pil);
                }
                while (pageBars[idxBar]->size() < idxPage) {
                    pageBars[idxBar]->append(-100);    // This barline is initially invisible
                    pageBarNums[idxBar]->append(-100); // It's bar number reflects that
                }
                pageBars[idxBar]->append(cellX - barRound);
                pageBarNums[idxBar]->append(n);
                idxBar++;
            }
        }
    } // for(measures)

    if (isHTML) { // Only one file, complete it, write it, and close it
        tableStream << HTML_BODY_END << endl << HTML_END << endl;
        tableStream.flush();
        tableFile.close();

        // Write the VTT file
        if (!saveVTT(score, fnRoot, tableCues))
            return false;

    }

    return true;
}


static QString spellUnicode(Segment* s, int idxTab, int idxTPC, unsigned int idxNote, Note* note)
{
    ChordRest* cr;
    Chord*     c;
    int        tpc;

    tpc = note->tpc1(); // fallback default value

    if (idxTab != idxTPC) {
        cr = s->cr(idxTPC * VOICES);
        if (cr != 0 && cr->type() == EType::CHORD) {
            c = static_cast<Chord*>(cr);
            if (idxNote < c->notes().size())
                tpc = c->notes()[idxNote]->tpc1();
        }
    }

    return tpc2unicode(tpc, NoteSpellingType::STANDARD, NoteCaseType::UPPER);
}

static QString getPickPosition(ChordRest* cr)
{
    foreach(Articulation* a, cr->articulations()) {
          if (a->articulationType() == ArticulationType::Upbow)
              return "upPick";
    }
    return "dnPick";
}

//
// saveSMAWS_Frets()
//
// For a single tablature staff. Number of strings == different template.
// If the score has more than one staff, use selected (tabs) staff, (Else fail)
// Saves 1 SVG file and 1 VTT files. The cues are all zero-duration.
// MuseScore has a number of frets property, but it's any integer up to 24.
// Until I can dynamically exclude extra frets from a 24-fret template, I will
// requires a separate menu item for each template's number of frets.
// I'm starting with two 12-fret templates: 6-string and 4-string.
// It's awkward that both guitars and c++ have strings...
// NOTE: Only zero-duration cues. There is apparently no need for duration here.
bool MuseScore::saveSMAWS_Frets(Score* score, QFileInfo* qfi)
{
    if (score->metaTag(tagWorkNo).isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Music"), tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }

    Staff*      staff;
    Measure*    m;
    Segment*    s;
    ChordRest*  cr;
    QString*    pqs;
    QTextStream qts;
    int         i;
    int         str;
    int         nStrings;
    int         tick;
    int         lastTick;
    int         tmp;
    int         width  = 0;
    int         height = 0;
    IntVect     stavesTab; // tablature staff indices
    IntVect     stavesTPC; // standard notation staff indices for note spelling, if available
    IntVect     xOffsets;
    IntListVect lastTicks;
    IntList*    pil;
    IntSet      setVTT;
    StrPtrVect*    spv;
    StrPtrVectList values;
    StrPtrVectList tunings;
    const int   SVG_FRET_NO = -999; // noLite y-coord for fingers. No headstock will be that tall.
    const int   MARGIN      = 20;   // margin between staves and for fret numbers
    const qreal RULE_OF_18  = 17.817154;

    // What staves are we using? Does the tab staff pair with a notation staff?
    tmp = 0;
    for (i = 0; i < score->nstaves(); i++) {
        staff = score->staves()[i];
        if (staff->isTabStaff()) {
            nStrings = staff->staffType()->lines();
            spv = new StrPtrVect(nStrings);
            for (str = 0; str < nStrings; str++)
                (*spv)[str] = new QString();
            values.append(spv);

            pil = new IntList();
            for (str = 0; str < nStrings; str++)
                pil->append(0);
            lastTicks.append(pil);

            stavesTab.append(i);
            if (i > 0 && score->staves()[i - 1]->part()->longName() ==
                                          staff->part()->longName().toLower())
                stavesTPC.append(i - 1);
            else
                stavesTPC.append(i);

            // Open string (noLite) unicode text values
            spv = new StrPtrVect(nStrings);
            for (str = 0; str < nStrings; str++)
                (*spv)[str] = new QString(
                                    tpc2unicode(
                                      pitch2tpc(
                                        staff->part()->instrument()->stringData()->stringList()[nStrings - str - 1].pitch, // the list itself is in opposite order from note.string() return value - messy, but true
                                        score->staves()[stavesTPC[tmp]]->key(0),
                                        Prefer::NEAREST),
                                      NoteSpellingType::STANDARD,
                                      NoteCaseType::UPPER));
            tunings.append(spv);

            // x offset for this fretboard, accumulated across staves as width
            xOffsets.append((nStrings * 21) + MARGIN + 6);   // fret numbers + 6=borders
            width += xOffsets[tmp] + (tmp > 0 ? MARGIN : 0); // "staff" margin
            tmp++;
        }
    }

    // Iterate chronologically left-to-right, staves top-to-bottom
    //   by measure, by segment of type ChordRest
    for (m = score->firstMeasure(); m; m = m->nextMeasureMM())
    {
        for (s = m->first(Segment::Type::ChordRest); s; s = s->next(Segment::Type::ChordRest))
        {
            for (i = 0; i < stavesTab.size(); i++)
            {
                cr = s->cr(stavesTab[i] * VOICES); // Only one voice handled at this time. Who has multi-voice tablature anyway? Why?
                if (cr == 0)
                    continue;

                tick = cr->tick();

                if (cr->type() == EType::CHORD) {
                    unsigned int n;
                    Note* note;
                    std::vector<Note*> notes = static_cast<Chord*>(cr)->notes();
                    for (n = 0; n < notes.size(); n++) {
                        // Store the start tick by stringNumber, along with 3 values:
                        //   tuning (textContent)
                        //   finger (fretNumber)
                        //   pick   (xlink:href)
                        //
                        // e.g. 0000000_0000000 E 5 dnPick,
                        //
                        note = notes[n];
                        str  = note->string();
                        pqs  = (*values[i])[str]; // Select the appropriate QString*
                        qts.setString(pqs);

                        // Insert string-level rests, noLite cues by string
                        pil = lastTicks[i];
                        lastTick = (*pil)[str];

                        if (lastTick < tick) { // Insert a string noLite cue
                            setVTT.insert(lastTick);
                            qts << (pqs->isEmpty() ? SVG_CUE : QString(SVG_COMMA))
                                << lastTick            << SVG_SPACE
                                << *(*tunings[i])[str] << SVG_SPACE
                                << SVG_FRET_NO         << SVG_SPACE
                                << "noPick";
                        }

                        (*pil)[str] = tick + cr->actualTicks();

                        setVTT.insert(tick);
                        qts << (pqs->isEmpty() ? SVG_CUE : QString(SVG_COMMA))
                            << tick         << SVG_SPACE
                            << spellUnicode(s, stavesTab[i], stavesTPC[i], n, note)
                                            << SVG_SPACE
                            << note->fret() << SVG_SPACE
                            << getPickPosition(cr);
                    }
                }
            }
        }
    }

    // Terminate the values
    lastTick = score->lastSegment()->tick();
    for (i = 0; i < values.size(); i++) {
        spv = values[i];
        pil = lastTicks[i];
        for (str = 0; str < spv->size(); str++) {
            tick = (*pil)[str];
            pqs  = (*spv)[str];
            qts.setString(pqs);
            if (tick < lastTick) {
                qts << (pqs->isEmpty() ? SVG_CUE : QString(SVG_COMMA))
                    << tick                << SVG_SPACE
                    << *(*tunings[i])[str] << SVG_SPACE
                    << SVG_FRET_NO         << SVG_SPACE
                    << "noPick";
            }
            qts << SVG_QUOTE;
        }
    }

    // Each fretboard is a <g> in a single _Frets.svg file
    QFile   qfSVG;
    QFile   qf;
    QString qs;
    QString fileName;
    QTextStream file;
    const QString fnRoot = QString("%1/%2%3%4").arg(qfi->path())
                                               .arg(score->metaTag(tagWorkNo))
                                               .arg(SMAWS_)
                                               .arg(SMAWS_FRETS);
    // The .svg file
    fileName = QString("%1%2").arg(fnRoot).arg(EXT_SVG);
    qfSVG.setFileName(fileName);
    qfSVG.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    file.setDevice(&qfSVG);

    // The headers
    height = 894; //!!! 14-fret height. With diff # of frets per staff, this must become very dynamic!!!
    file << SVG_BEGIN      // <svg>
            << XML_NAMESPACE << XML_XLINK << SVG_4SPACES
            << SVG_VIEW_BOX  << SVG_ZERO  << SVG_SPACE
                             << SVG_ZERO  << SVG_SPACE
                             << width     << SVG_SPACE
                             << height    << SVG_QUOTE
            << SVG_WIDTH     << width     << SVG_QUOTE
            << SVG_HEIGHT    << height    << SVG_QUOTE
                                                << endl   << SVG_4SPACES
            << SVG_PRESERVE_XYMIN_MEET          << endl   << SVG_4SPACES
            << SVG_POINTER_EVENTS << SVG_CURSOR << endl   << SVG_4SPACES
            << SVG_CLASS << SMAWS << SVG_QUOTE  << SVG_GT << endl;

    // The <defs>
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_FRET_DEFS));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);
    file << qts.readAll();

    // The staves as fretboards
    tmp = 0;
    for (i = 0; i < values.size(); i++) {
        // Which template file? (eventually they can be built more dynamically...)
        nStrings = values[i]->size();
        switch (nStrings) {
        case 6:
            fileName = FILE_FRETS_14_6;
            break;
        case 4:
            fileName = FILE_FRETS_14_4;
            break;
        default:
            continue; // not currently supported
        }

        // Replace the strings in the template with the values in the vector
        qs = QString("%1/%2").arg(qfi->path()).arg(fileName);
        qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(fileName));
        qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
        qts.setDevice(&qf);

        tmp += (i > 0 ? xOffsets[i - 1] + 20: 0); // 20 = margin between staves
        qs = qts.readAll();
        qs.replace("%id", score->staves()[stavesTab[i]]->part()->longName(0)); // staff id attribute, same as tab staff in score
        qs.replace("%x", QString::number(tmp));  // The tranlate(x 0) coordinate for this staff - fretboards are initially vertical, stacked horizontally
        for (str = 0; str < nStrings; str++)
            qs.replace(QString("\%%1").arg(str), *(*values[i])[str]);

        file << qs;
    }

    // The buttons (best last, as they must always be on top)
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_FRET_BUTTS));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);
    file << qts.readAll();

    file << SVG_END;                      // Terminate the <svg>
    file.flush();                         // Write the file
    qfSVG.close();                        // Close the SVG file
    qf.close();                           // Close the read-only file
    saveStartVTT(score, fnRoot, &setVTT); // Write the VTT file
    return true;                          // Return success
}

//
// saveSMAWS_Tree()
// Special characters prefixed to node names to differentiate cue types
//   ,  separates nodes within a cue
//   !  ruler gray-out cue
///// Not implemented yet, if ever, but good ideas:
/////^  hide node cue
/////*  show node cue
/////&  change node name cue
/////=  not a prefix, separates old node name from new one
//
bool MuseScore::saveSMAWS_Tree(Score* score, QFileInfo* qfi)
{
    if (score->metaTag(tagWorkNo).isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Tree"), tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }

    QMultiMap<QString, QString> mapMix; // key = CueID, value = InstrumentName
    const char notChar = '!'; // used only once, but I deplore raw literals

    // Iterate by Staff, by ChordRest, collecting into mapMix
    for (int t = 0, max = score->nstaves() * VOICES;
             t < max;
             t += VOICES)
    {
        // Staff index
        const int idx = t / VOICES;

        // These are smaller in SVG too
        const bool isPulse = score->staff(idx)->small();

        // 1 Intrument Name = comma-separated list of MixTree node names
        const QString iName = score->systems().first()->staff(idx)->instrumentNames.first()->xmlText();

        // The "not" (gray-out) version of the instrument name.
        // Each node name must be prefixed with the notChar in the cue text.
        QStringList notList = iName.split(SVG_COMMA);
        QString notName;
        QTextStream qts(&notName);
        for (QStringList::iterator i = notList.begin(); i != notList.end(); ++i) {
            if (i != notList.begin())
                qts << SVG_COMMA; // Comma-delimited, but not comma-terminated
            qts << notChar << *i;
        }

        int      startTick  = 0;
        bool     isPrevRest = true; // Is the previous element a rest?
        Measure* pm = 0;            // The previous measure
        Note*    note;

        // By measure by segment of type ChordRest
        for (Measure* m = score->firstMeasure();
                      m;
                      m = m->nextMeasureMM())
        {
            // Empty measure = all rests
            // Empty measures generate gray-out cues for the timeline rulers.
            if (m->isMeasureRest(idx)) {
                // If there is a highlight cue pending, finish it.
                // --Duplicates code from below, see: "Case: EType::Rest".--
                if (!isPulse && !isPrevRest && startTick != 0) {
                    isPrevRest = true;
                    mapMix.insert(getCueID(startTick, m->tick()), iName);
                }

                // Empty measure following a non-empty measure. Start of
                // a possibly multi-measure rest.
                if (pm != 0 && !pm->isMeasureRest(idx))
                    startTick = m->tick();

                // Final measure is empty
                if (!m->nextMeasureMM())
                    mapMix.insert(getCueID(startTick, m->tick() + m->ticks()), notName);
            }
            // Non-empty measures have highlight and pulse cues
            else {
                // Complete any pending gray-out cue
                if (pm != 0 && pm->isMeasureRest(idx))
                    mapMix.insert(getCueID(startTick, m->tick()), notName);

                // Highlight and Pulse cues
                for (Segment* s = m->first(Segment::Type::ChordRest);
                              s;
                              s = s->next(Segment::Type::ChordRest))
                {
                    ChordRest* cr = s->cr(t);
                    if (cr == 0)
                        continue;
                    switch (cr->type()) {
                    case EType::CHORD :
                        if (isPulse || isPrevRest) {
                            isPrevRest = false;
                            startTick = cr->tick();
                        }
                        if (isPulse) {
                            note = static_cast<Chord*>(cr)->notes()[0];
                            while (note->tieFor() && note != note->tieFor()->endNote())
                                note = note->tieFor()->endNote();
                            cr = static_cast<ChordRest*>(note->parent());
                            mapMix.insert(getCueID(startTick, cr->tick() + cr->actualTicks()), iName);
                            s = cr->segment(); // Â¡Updates the pointer used in the inner for loop!
                        }
                        break;
                    case EType::REST :
                        if (!isPulse && !isPrevRest && startTick != 0) {
                            isPrevRest = true;
                            mapMix.insert(getCueID(startTick, cr->tick()), iName);
                        }
                        break;
                    default:
                        break; // This should never happen
                    }
                }
            }
            pm = m; // Remember the previous measure for the next iteration
        }
        // If the chord lasts until the end of the score
        if (!isPulse && !isPrevRest)
            mapMix.insert(getCueID(startTick, score->lastSegment()->tick()), iName);
    }


    // Open a stream into the file
    QFile fileVTT;
    fileVTT.setFileName(QString("%1/%2%3%4%5").arg(qfi->path())
                                              .arg(score->metaTag(tagWorkNo))
                                              .arg(SMAWS_)
                                              .arg(score->metaTag(tagMoveNo))
                                              .arg(EXT_VTT));
    fileVTT.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    QTextStream streamVTT(&fileVTT);

    // Stream the header
    streamVTT << VTT_HEADER;

    // Stream the cues, iterating by cue_id
    const TempoMap* tempos = score->tempomap();
    const QStringList keys = mapMix.uniqueKeys();
    for (QStringList::const_iterator cue_id  = keys.cbegin();
                                     cue_id != keys.cend();
                                   ++cue_id)
    {
        const QStringList values = mapMix.values(*cue_id); // QStringList::join()

        // Stream the cue: 0000000_1234567
        //                 00:00:00.000 --> 12:34:56.789
        //                 Instrument1,Instrument2,...,InstrumentN
        //                [this line intentionally left blank, per WebVTT spec]

        streamVTT << getVTTCueTwo(*cue_id, tempos);  // First two lines
        streamVTT << values.join(SVG_COMMA) << endl; // Instrument list
        streamVTT << endl;                           // Blank line
    }

    // Write and close the Tree VTT file
    streamVTT.flush();
    fileVTT.close();
    return true;
}

//
// saveSMAWS_Lyrics() - produces one VTT file and one SVG file.
// All staves are exported to VTT.
// Only staves with .hideSystemBarLine() == true are exported to SVG.
// Multiple staves are consolidated into one flow in the SVG file.
// The possibility of multiple columns of lyrics highlighting
// independently has some appeal, but it it not implemented yet.
//
bool MuseScore::saveSMAWS_Lyrics(Score* score, QFileInfo* qfi)
{
    if (score->metaTag(tagWorkNo).isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Lyrics"), tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }

    const TempoMap* tempos          = score->tempomap();
    const QString   VTT_CLASS_BEGIN = "<c.";
    const QString   VTT_CLASS_END   = "</c>";

    int     i;
    int     tick;
    int     startTick  = 0;
    bool    isSVG;
    bool    isRest;
    bool    isOmit;
    bool    isPrevRest;
    bool    isPrevOmit;
    bool    isItalic;
    QString lyricsStaff;
    QString lyricsItalic;
    QString lyricsEmpty;
    QString lyricsVTT;
    QString lyricsSVG;
    IntSet  setVTT;                     // std::set of mapSVG.uniqueKeys()
    QMultiMap<QString, QString> mapVTT; // subtitles:  key = cue_id,    value = lyrics text
    QMultiMap<int,     QString> mapSVG; // lyrics svg: key = startTick, value = lyrics text

    const QString italicTag = "<i>";
    Articulation* art = new Articulation(score);
    art->setArticulationType(ArticulationType::Downbow);

    // Iterate over the lyrics staves and collect cue_ids + lyrics text
    // By staff by measure by segment of type ChordRest
    for (i = 0; i < score->nstaves(); i++)
    {
        isPrevRest = true;
        isSVG      = score->staff(i)->hideSystemBarLine(); // it's a reasonable property to hijack for this purpose

        // VTT+CSS-class version of part name, enclosed in angle brackets.
        // VTT cue text looks like this:
        //     <c.partName>insert-lyrics-here</c>
        // CSS rule looks like this:
        //     .partName {...}
        // VTT speaker name is a <v name></v> block (not implemented yet, use staff.instrument.shortName)
        lyricsStaff = QString("%1%2").arg(VTT_CLASS_BEGIN).arg(score->staff(i)->partName());

        for (Measure* m = score->firstMeasure(); m; m = m->nextMeasureMM()) {
            for (Segment* s = m->first(Segment::Type::ChordRest); s; s = s->next(Segment::Type::ChordRest))
            {
                ChordRest* cr = s->cr(i * VOICES);
                if (cr == 0)
                    continue;

                // Chords with downbow articulations are treated like rests for
                // the SVG file. Actual rests shouldn't have articulations...
                isOmit = cr->hasArticulation(art);

                tick   = cr->tick();
                isRest = cr->isRest();
                if (!isRest) { // It's a Chord
                    if (cr->lyrics().size() > 0) {
                        if (isPrevRest) { // New line of lyrics
                            startTick  = tick;
                            isItalic   = cr->lyrics()[0]->textStyle().italic();
                            lyricsItalic = (isItalic ? "Italic" : "");
                            lyricsVTT    = lyricsStaff + lyricsItalic + SVG_SPACE;
                            if (isSVG && !isOmit) {
                                lyricsSVG = (isItalic ? italicTag : "");
                            }
                        }
                        else {            // Add to existing line of lyrics
                            switch (cr->lyrics()[0]->syllabic()) {
                            case Lyrics::Syllabic::SINGLE :
                            case Lyrics::Syllabic::BEGIN  :
                                lyricsVTT += SVG_SPACE;   // new word, precede it with a space
                                if (isSVG && !isOmit)
                                    lyricsSVG += SVG_SPACE;
                                break;
                            default :
                                break; // multi-syllable word with separate timestap tags, no preceding space
                            }
                        }
                        lyricsVTT += SVG_LT;
                        lyricsVTT += ticks2VTTmsecs(tick, tempos);
                        lyricsVTT += SVG_GT;
                        lyricsVTT += cr->lyrics()[0]->plainText(); ///!!! looping over lyricsList vector will be necessary soon, tied in with repeats, which are also 100% unhandled in SMAWS today!!! though for animated scores it may not be necessary.  Repeats require multiple cue_ids per animated note.
                        if (isSVG && !isOmit)
                            lyricsSVG += cr->lyrics()[0]->plainText();
                    }
                }
                else { // It's a rest
                    if (!isPrevRest && tick != 0) {
                        lyricsVTT += VTT_CLASS_END;
                        mapVTT.insert(getCueID(startTick, tick), lyricsVTT);

                        if (isSVG && !isPrevOmit) {
                            mapSVG.insert(startTick, lyricsSVG);
                            setVTT.insert(startTick);
                        }
                    }

                    // RehearsalMarks are used to create non-lyrics lines in the
                    // SVG file. These are necessary to animate properly. They take
                    // care of the intro/outro/instrumental sections animation.
                    // SVG_LT distinguishes this from lyrics text in mapSVG.
                    if (isRest && isSVG) { // no good way to restrict this further, every rest in this staff...
                        for (Element* eAnn : s->annotations()) {
                            if (eAnn->type()  == EType::REHEARSAL_MARK
                             && setVTT.find(tick) == setVTT.end()) { // rehearsal marks are system-level, not staff-level
                                lyricsEmpty = static_cast<RehearsalMark*>(eAnn)->xmlText() + SVG_LT;
                                mapSVG.insert(tick, lyricsEmpty);
                                setVTT.insert(tick);
                                break; // only one marker per segment
                            }
                        }
                    }
                }

                isPrevRest = isRest; // for the next segment
                isPrevOmit = isOmit;

            } // for each segment
        } // for each measure
    } // for each staff

    // If the chord lasts until the end of the score
    if (!isPrevRest) {
        lyricsVTT += VTT_CLASS_END;
        mapVTT.insert(getCueID(startTick, score->lastSegment()->tick()), lyricsVTT);
        if (isSVG)
            mapSVG.insert(startTick, lyricsSVG);
            setVTT.insert(startTick);
    }

    // It's file time
    QString     qs;
    QFile       qf;
    QTextStream qts;
    QStringList keys;
    QStringList values;

    const QString fnRoot   = QString("%1/%2").arg(qfi->path()).arg(score->metaTag(tagWorkNo));
    const QString fnLyrics = QString("%1%2%3").arg(fnRoot).arg(SMAWS_).arg(SMAWS_LYRICS);

    // Subtitles VTT file
    qf.setFileName(QString("%1%2%3%4").arg(fnRoot).arg(SMAWS_).arg(SMAWS_VIDEO).arg(EXT_VTT));
    qf.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);

    // Stream the header
    qts << VTT_HEADER;

    // Stream the cues, iterating by cue_id
    keys = mapVTT.uniqueKeys();
    for (QStringList::iterator cue_id  = keys.begin();
                               cue_id != keys.end();
                             ++cue_id)
    {
        values = mapVTT.values(*cue_id); // QStringList::join()

        // Stream the cue: 0000000_1234567
        //                 00:00:00.000 --> 12:34:56.789
        //                 Lyrics Text
        //                 optional additional staves of lyrics text...
        //                [this line intentionally left blank, per WebVTT spec]
        qts << getVTTCueTwo(*cue_id, tempos); // First two lines
        qts << values.join("\n") << endl;     // Lyrics text
        qts << endl;                          // Blank line
    }

    // Write and close the VTT file
    qts.flush();
    qf.close();

    // SVG File assumes 14pt font, it's in the CSS for lyricsNo/Hi classes.
    // It's SVG, so it scales, thus the initial font size is not critical.
    const int startY    = 16;
    const int increment = 18;
    const int offset    = increment - 4; // bottom line + 4, but y is bottom line + increment at the time when this is used
    const int lyricsX   = 24; // a fixed value for the x attribute of every text element - formatting done after this export...
    const int maxDigits =  4; // max y value for vertically aligned number formatting == 9999, extremely reasonable value

    int y = startY;

    QTextStream qtsRead;  // SVG file is fully based on a template file
    QString     className;

    // Collect the cues into a string, iterating by cue_id, and calculating y
    lyricsSVG.clear();
    qts.setString(&lyricsSVG);
    for (IntSet::iterator tick = setVTT.begin(); tick != setVTT.end(); ++tick) {
        values = mapSVG.values(*tick);
        for (QStringList::iterator i = values.begin(); i != values.end(); ++i) {
            if (i->startsWith(italicTag)) {
                i->remove(0, italicTag.size());
                lyricsItalic = " font-style=\"italic\"";
            }
            else
                lyricsItalic = "";

            if (i->endsWith(SVG_LT)) {
                i->resize(i->size() - 1); // remove the SVG_LT
                className = "lyricsMt";   // Mt == Empty
            }
            else
                className = "lyricsNo";

            qts << SVG_8SPACES << SVG_TEXT_BEGIN
                   << formatInt(SVG_X,        lyricsX, maxDigits,        true)
                   << formatInt(SVG_Y,        y,       maxDigits,        true)
                   << formatInt(SVG_CUE_NQ,   *tick, CUE_ID_FIELD_WIDTH, true)
                   << SVG_CLASS << className  << SVG_QUOTE
                   << lyricsItalic
                   << SVG_GT    << *i
                << SVG_TEXT_END << endl;

            y += increment;
        }
    }

    // Open the template file for read-only
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_LYRICS));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qtsRead.setDevice(&qf);
    qs = qtsRead.readAll(); // the unpopulated template is small, < 400 bytes

    // Open a stream into the SVG file
    qf.setFileName(QString("%1%2").arg(fnLyrics).arg(EXT_SVG));
    qf.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);

    qts << qs.replace("%1", QString::number(999)) ///!!!I have no way to calculate width here
             .replace("%2", QString::number(y - offset))
             .replace("%3", score->title())
             .replace("%4", smawsDesc(score))
             .replace("%5", lyricsSVG);

    // Write and close the SVG file
    qts.flush();
    qf.close();

    // The SVG lyrics VTT file generation is pre-encapsulated (shared by frets)
    saveStartVTT(score, fnLyrics, &setVTT);

    delete art; // the friendly thing to do

    return true;
}

// End SVG / SMAWS
///////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//   createThumbnail
//---------------------------------------------------------

static QPixmap createThumbnail(const QString& name)
      {
      MasterScore* score = new MasterScore;
      Score::FileError error = readScore(score, name, true);
      if (error != Score::FileError::FILE_NO_ERROR) {
            delete score;
            return QPixmap();
      }
      score->doLayout();
      QImage pm = score->createThumbnail();
      delete score;
      return QPixmap::fromImage(pm);
      }

//---------------------------------------------------------
//   extractThumbnail
//---------------------------------------------------------

QPixmap MuseScore::extractThumbnail(const QString& name)
      {
      QPixmap pm; //  = icons[File_ICON].pixmap(QSize(100,140));
      if (!name.endsWith(".mscz"))
            return createThumbnail(name);
      MQZipReader uz(name);
      if (!uz.exists()) {
            qDebug("extractThumbnail: <%s> not found", qPrintable(name));
            return pm;
            }
      QByteArray ba = uz.fileData("Thumbnails/thumbnail.png");
      if (ba.isEmpty())
            return createThumbnail(name);
      pm.loadFromData(ba, "PNG");
      return pm;
      }
}
