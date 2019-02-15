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
#include "libmscore/stafflines.h"
#include "synthesizer/msynthesizer.h"
#include "svggenerator.h"
#include "scorePreview.h"
#include "scorecmp/scorecmp.h"
#include "extension.h"
#include "tourhandler.h"
#include "libmscore/articulation.h"
#include "libmscore/rehearsalmark.h"

#ifdef OMR
#include "omr/omr.h"
#include "omr/omrpage.h"
#include "omr/importpdf.h"
#endif

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
#define SMAWS_TREE   "Tree"
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
#define EXT_TEXT ".txt"

// Template files
#define FILE_RULER_HDR  "templates/SMAWS_RulerHdr.svg.txt"   // ruler svg boilerplate file header/title
#define FILE_RULER_FTR  "templates/SMAWS_RulerFtr.svg.txt"   // ditto content footer with loop cursors
#define FILE_RULER_DEFS "templates/SMAWS_RulerDefs.svg.txt"  // ditto <defs>
#define FILE_RULER_RB   "templates/SMAWS_RulerRectB.svg.txt" // invisible rect element for bars ruler
#define FILE_RULER_RM   "templates/SMAWS_RulerRectM.svg.txt" // invisible rect element for markers ruler
#define FILE_RULER_TB   "templates/SMAWS_RulerTextB.svg.txt" // text element for bars ruler
#define FILE_RULER_TM   "templates/SMAWS_RulerTextM.svg.txt" // text element for markers ruler
#define FILE_PLAY_BUTTS "templates/SMAWS_PlayButts.svg.txt"  // sheet music playback buttons
#define FILE_FRET_DEFS  "templates/SMAWS_FretsDefs.svg.txt"  // Fretboard <defs>
#define FILE_FRET_BUTTS "templates/SMAWS_FretsButts.svg.txt" // Fretboard buttons
#define FILE_FRETS_12_6 "templates/SMAWS_Frets12-6.svg.txt"  // 12-fret, 6-string fretboard template
#define FILE_FRETS_12_4 "templates/SMAWS_Frets12-4.svg.txt"  // 12-fret, 4-string fretboard template
#define FILE_FRETS_14_6 "templates/SMAWS_Frets14-6.svg.txt"  // 14-fret, 6-string fretboard template
#define FILE_FRETS_14_4 "templates/SMAWS_Frets14-4.svg.txt"  // 14-fret, 4-string fretboard template
#define FILE_GRID_DEFS  "templates/SMAWS_GridDefs.svg.txt"   // Grid <defs>
#define FILE_GRID_BG    "templates/SMAWS_GridBg.svg.txt"     // Grid background elements
#define FILE_GRID_TEMPO "templates/SMAWS_GridTempo.svg.txt"  // Grid page buttons
#define FILE_GRID_INST  "templates/SMAWS_GridInst.svg.txt"   // Grid controls by row/channel
#define FILE_GRID_PLAY  "templates/SMAWS_GridPlayButts.svg.txt" // Grid playback buttons
#define FILE_GRID_BOTH  "templates/SMAWS_GridPageButts.svg.txt" // Grid playback + page buttons

#define FILTER_SMAWS_AUTO_OPEN   "Auto-SMAWS: Open Files"
#define FILTER_SMAWS_AUTO_ALL    "Auto-SMAWS:  All Files"
#define FILTER_SMAWS             "SMAWS Part"
#define FILTER_SMAWS_MULTI       "SMAWS Score"
#define FILTER_SMAWS_RULERS      "SMAWS Rulers"
#define FILTER_SMAWS_TABLES      "SMAWS HTML Tables"
#define FILTER_SMAWS_GRID        "SMAWS Grids"
#define FILTER_SMAWS_GRID_RULERS "SMAWS Grids w/built-in Rulers"
#define FILTER_SMAWS_FRETS       "SMAWS Fretboards"
#define FILTER_SMAWS_MIX_TREE    "SMAWS MixTree"
#define FILTER_SMAWS_LYRICS      "SMAWS Lyrics"
#define FILTER_SMAWS_TOUR        "SMAWS Guided Tour"

#define SMAWS_DESC_STUB "&#xA9;%1 %2 - generated by MuseScore %3 + SMAWS&#x2122; %4"

#define VTT_CUE_3_ARGS  "%1\n%2 --> %3\n" // for Cue ID formatting

// For Frozen Pane formatting
#define WIDTH_CLEF     16
#define WIDTH_KEY_SIG   5
#define WIDTH_TIME_SIG 10
#define X_OFF_TIME_SIG  3

// SMAWS end
//////////////////////////////////

namespace Ms {

extern void importSoundfont(QString name);

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
//printf("<%s> %d\n", qPrintable(name), int(error));
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
                              .arg("<a href=\"https://musescore.org/download#older-versions\">")
                              .arg("</a>");
                  canIgnore = true;
                  break;
            case Score::FileError::FILE_TOO_NEW:
                  msg += QObject::tr("This score was saved using a newer version of MuseScore.\n"
                                     "Visit the %1MuseScore website%2 to obtain the latest version.")
                              .arg("<a href=\"https://musescore.org\">")
                              .arg("</a>");
                  canIgnore = true;
                  break;
            case Score::FileError::FILE_NOT_FOUND:
                  msg = QObject::tr("File \"%1\" not found.").arg(name);
                  break;
            case Score::FileError::FILE_CORRUPTED:
                  msg = QObject::tr("File \"%1\" corrupted.").arg(name);
                  detailedMsg = MScore::lastError;
                  canIgnore = true;
                  break;
            case Score::FileError::FILE_OLD_300_FORMAT:
                  msg += QObject::tr("It was last saved with a developer version of 3.0.\n");
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
      msgBox.setWindowTitle(QObject::tr("Load Error"));
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

bool MuseScore::checkDirty(MasterScore* s)
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

void MuseScore::loadFiles(bool switchTab, bool singleFile)
      {
      QStringList files = getOpenScoreNames(
#ifdef OMR
         tr("All Supported Files") + " (*.mscz *.mscx *.mxl *.musicxml *.xml *.mid *.midi *.kar *.md *.mgu *.sgu *.cap *.capx *.pdf *.ove *.scw *.bww *.gtp *.gp3 *.gp4 *.gp5 *.gpx);;" +
#else
         tr("All Supported Files") + " (*.mscz *.mscx *.mxl *.musicxml *.xml *.mid *.midi *.kar *.md *.mgu *.sgu *.cap *.capx *.ove *.scw *.bww *.gtp *.gp3 *.gp4 *.gp5 *.gpx);;" +
#endif
         tr("MuseScore Files") + " (*.mscz *.mscx);;" +
         tr("MusicXML Files") + " (*.mxl *.musicxml *.xml);;" +
         tr("MIDI Files") + " (*.mid *.midi *.kar);;" +
         tr("MuseData Files") + " (*.md);;" +
         tr("Capella Files") + " (*.cap *.capx);;" +
         tr("BB Files (experimental)") + " (*.mgu *.sgu);;" +
#ifdef OMR
         tr("PDF Files (experimental OMR)") + " (*.pdf);;" +
#endif
         tr("Overture / Score Writer Files (experimental)") + " (*.ove *.scw);;" +
         tr("Bagpipe Music Writer Files (experimental)") + " (*.bww);;" +
         tr("Guitar Pro") + " (*.gtp *.gp3 *.gp4 *.gp5 *.gpx)",
         tr("Load Score"),
         singleFile
         );
      for (const QString& s : files)
            openScore(s, switchTab);
      mscore->tourHandler()->showDelayedWelcomeTour();
      }

//---------------------------------------------------------
//   openScore
//---------------------------------------------------------

Score* MuseScore::openScore(const QString& fn, bool switchTab)
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
            score->updateCapo();
            const int tabIdx = appendScore(score);
            if (switchTab)
                  setCurrentScoreView(tabIdx);
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

      MasterScore* score = new MasterScore(MScore::defaultStyle());
      setMidiReopenInProgress(name);
      Score::FileError rv = Ms::readScore(score, name, false);
      if (rv == Score::FileError::FILE_TOO_OLD || rv == Score::FileError::FILE_TOO_NEW || rv == Score::FileError::FILE_CORRUPTED) {
            if (readScoreError(name, rv, true)) {
                  if (rv != Score::FileError::FILE_CORRUPTED) {
                        // don’t read file again if corrupted
                        // the check routine may try to fix it
                        delete score;
                        score = new MasterScore();
                        score->setMovements(new Movements());
                        score->setStyle(MScore::defaultStyle());
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

bool MuseScore::saveFile(MasterScore* score)
      {
      if (score == 0)
            return false;
      if (score->created()) {
            QString fn = score->masterScore()->fileInfo()->fileName();
            Text* t = score->getText(Tid::TITLE);
            if (t)
                  fn = t->plainText();
            QString name = createDefaultFileName(fn);
            QString f1 = tr("MuseScore File") + " (*.mscz)";
            QString f2 = tr("Uncompressed MuseScore File") + " (*.mscx)";

            QSettings set;
            if (mscore->lastSaveDirectory.isEmpty())
                  mscore->lastSaveDirectory = set.value("lastSaveDirectory", preferences.getString(PREF_APP_PATHS_MYSCORES)).toString();
            QString saveDirectory = mscore->lastSaveDirectory;

            if (saveDirectory.isEmpty())
                  saveDirectory = preferences.getString(PREF_APP_PATHS_MYSCORES);

            QString fname = QString("%1/%2").arg(saveDirectory).arg(name);
            QString filter = f1 + ";;" + f2;
            if (QFileInfo(fname).suffix().isEmpty())
                  fname += ".mscz";

            fn = mscore->getSaveScoreName(tr("Save Score"), fname, filter);
            if (fn.isEmpty())
                  return false;
            score->masterScore()->fileInfo()->setFile(fn);

            mscore->lastSaveDirectory = score->masterScore()->fileInfo()->absolutePath();

            if (!score->masterScore()->saveFile()) {
                  QMessageBox::critical(mscore, tr("Save File"), MScore::lastError);
                  return false;
                  }
            addRecentScore(score);
            writeSessionFile(false);
            }
      else if (!score->masterScore()->saveFile()) {
            QMessageBox::critical(mscore, tr("Save File"), MScore::lastError);
            return false;
            }
      score->setCreated(false);
      updateWindowTitle(score);
      scoreCmpTool->updateScoreVersions(score);
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
            for (MasterScore* s : scoreList) {
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

//---------------------------------------------------------
//   getNewFile
//    create new score
//---------------------------------------------------------

MasterScore* MuseScore::getNewFile()
      {
      if (!newWizard)
            newWizard = new NewWizard(this);
	  else {
            newWizard->updateValues();
            newWizard->restart();
            }
      if (newWizard->exec() != QDialog::Accepted)
            return 0;
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

      MasterScore* score = new MasterScore(MScore::defaultStyle());
      QString tp         = newWizard->templatePath();

      QList<Excerpt*> excerpts;
      if (!newWizard->emptyScore()) {
            MasterScore* tscore = new MasterScore(MScore::defaultStyle());
            Score::FileError rv = Ms::readScore(tscore, tp, false);
            if (rv != Score::FileError::FILE_NO_ERROR) {
                  readScoreError(newWizard->templatePath(), rv, false);
                  delete tscore;
                  delete score;
                  return 0;
                  }
            score->setStyle(tscore->style());

            // create instruments from template
            for (Part* tpart : tscore->parts()) {
                  Part* part = new Part(score);
                  part->setInstrument(tpart->instrument());
                  part->setPartName(tpart->partName());

                  for (Staff* tstaff : *tpart->staves()) {
                        Staff* staff = new Staff(score);
                        staff->setPart(part);
                        staff->init(tstaff);
                        if (tstaff->links() && !part->staves()->isEmpty()) {
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
            if (mb && mb->isVBox()) {
                  VBox* tvb = toVBox(mb);
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
      score->fileInfo()->setFile(createDefaultName());

      if (!score->style().chordList()->loaded()) {
            if (score->styleB(Sid::chordsXmlFile))
                  score->style().chordList()->read("chords.xml");
            score->style().chordList()->read(score->styleSt(Sid::chordDescriptionFile));
            }
      if (!newWizard->title().isEmpty())
            score->fileInfo()->setFile(newWizard->title());

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
                        measure->setIrregular(true);        // don’t count pickup measure
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
                              Segment* s = m->getSegment(SegmentType::TimeSig, 0);
                              s->add(ts);
                              Part* part = staff->part();
                              if (!part->instrument()->useDrumset()) {
                                    //
                                    // transpose key
                                    //
                                    KeySigEvent nKey = ks;
                                    if (!nKey.custom() && !nKey.isAtonal() && part->instrument()->transpose().chromatic && !score->styleB(Sid::concertPitch)) {
                                          int diff = -part->instrument()->transpose().chromatic;
                                          nKey.setKey(transposeKey(nKey.key(), diff));
                                          }
                                    // do not create empty keysig unless custom or atonal
                                    if (nKey.custom() || nKey.isAtonal() || nKey.key() != Key::C) {
                                          staff->setKey(0, nKey);
                                          KeySig* keysig = new KeySig(score);
                                          keysig->setTrack(staffIdx * VOICES);
                                          keysig->setKeySigEvent(nKey);
                                          Segment* ss = measure->getSegment(SegmentType::KeySig, 0);
                                          ss->add(keysig);
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
                                          Segment* seg = measure->getSegment(SegmentType::ChordRest, ltick);
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
                              Segment* seg = measure->getSegment(SegmentType::ChordRest, tick);
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
            if (s->segmentType() == SegmentType::ChordRest) {
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
            if (measure->type() != ElementType::VBOX) {
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
                  Text* s = new Text(score, Tid::TITLE);
                  s->setPlainText(title);
                  measure->add(s);
                  score->setMetaTag("workTitle", title);
                  }
            if (!subtitle.isEmpty()) {
                  Text* s = new Text(score, Tid::SUBTITLE);
                  s->setPlainText(subtitle);
                  measure->add(s);
                  }
            if (!composer.isEmpty()) {
                  Text* s = new Text(score, Tid::COMPOSER);
                  s->setPlainText(composer);
                  measure->add(s);
                  score->setMetaTag("composer", composer);
                  }
            if (!poet.isEmpty()) {
                  Text* s = new Text(score, Tid::POET);
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
            Segment* seg = score->firstMeasure()->first(SegmentType::ChordRest);
            seg->add(tt);
            score->setTempo(0, tempo);
            }
      if (!copyright.isEmpty())
            score->setMetaTag("copyright", copyright);

      score->rebuildMidiMapping();

      {
            ScoreLoad sl;
            score->doLayout();
            }

      for (Excerpt* x : excerpts) {
            Score* xs = new Score(static_cast<MasterScore*>(score));
            xs->style().set(Sid::createMultiMeasureRests, true);
            x->setPartScore(xs);
            xs->setExcerpt(x);
            score->excerpts().append(x);
            Excerpt::createExcerpt(x);
            }
      score->setExcerptsChanged(true);
      return score;
      }

//---------------------------------------------------------
//   newFile
//    create new score
//---------------------------------------------------------

void MuseScore::newFile()
      {
      MasterScore* score = getNewFile();
      if (score)
            setCurrentScoreView(appendScore(score));
      mscore->tourHandler()->showDelayedWelcomeTour();
      }

//---------------------------------------------------------
//   copy
//    Copy content of src file do dest file overwriting it.
//    Implemented manually as QFile::copy refuses to
//    overwrite existing files.
//---------------------------------------------------------

static bool copy(QFile& src, QFile& dest)
      {
      src.open(QIODevice::ReadOnly);
      dest.open(QIODevice::WriteOnly);
      constexpr qint64 size = 1024 * 1024;
      char* buf = new char[size];
      bool err = false;
      while (qint64 r = src.read(buf, size)) {
            if (r < 0) {
                  err = true;
                  break;
                  }
            qint64 w = dest.write(buf, r);
            if (w < r) {
                  err = true;
                  break;
                  }
            }
      dest.close();
      src.close();
      delete[] buf;
      return !err;
      }

//---------------------------------------------------------
//   getTemporaryScoreFileCopy
//    baseNameTemplate is the template to be passed to
//    QTemporaryFile constructor but without suffix and
//    directory --- they are defined automatically.
//---------------------------------------------------------

QTemporaryFile* MuseScore::getTemporaryScoreFileCopy(const QFileInfo& info, const QString& baseNameTemplate)
      {
      QString suffix(info.suffix());
      if (suffix.endsWith(",")) // some backup files created by MuseScore
            suffix.chop(1);
      QTemporaryFile* f = new QTemporaryFile(
         QDir::temp().absoluteFilePath(baseNameTemplate + '.' + suffix),
         this
         );
      QFile src(info.absoluteFilePath());
      if (!copy(src, *f)) {
            delete f;
            return nullptr;
            }
      return f;
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
      QFileInfo myScores(preferences.getString(PREF_APP_PATHS_MYSCORES));
      urls.append(QUrl::fromLocalFile(myScores.absoluteFilePath()));
      urls.append(QUrl::fromLocalFile(QDir::currentPath()));
      return urls;
      }

//---------------------------------------------------------
//   getOpenScoreNames
//---------------------------------------------------------

QStringList MuseScore::getOpenScoreNames(const QString& filter, const QString& title, bool singleFile)
      {
      QSettings set;
      QString dir = set.value("lastOpenPath", preferences.getString(PREF_APP_PATHS_MYSCORES)).toString();
      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
            QStringList fileList = QFileDialog::getOpenFileNames(this,
               title, dir, filter);
            if (fileList.count() > 0) {
                  QFileInfo fi(fileList[0]);
                  set.setValue("lastOpenPath", fi.absolutePath());
                  }
            return fileList;
            }
      QFileInfo myScores(preferences.getString(PREF_APP_PATHS_MYSCORES));
      if (myScores.isRelative())
            myScores.setFile(QDir::home(), preferences.getString(PREF_APP_PATHS_MYSCORES));

      if (loadScoreDialog == 0) {
            loadScoreDialog = new QFileDialog(this);
            loadScoreDialog->setFileMode(singleFile ? QFileDialog::ExistingFile : QFileDialog::ExistingFiles);
            loadScoreDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            loadScoreDialog->setWindowTitle(title);
            addScorePreview(loadScoreDialog);

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
      // setup side bar urls
      QList<QUrl> urls = sidebarUrls();
      urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/demos"));
      loadScoreDialog->setSidebarUrls(urls);

      QStringList result;
      if (loadScoreDialog->exec())
            result = loadScoreDialog->selectedFiles();
      set.setValue("lastOpenPath", loadScoreDialog->directory().absolutePath());
      return result;
      }

//---------------------------------------------------------
//   getSaveScoreName
//---------------------------------------------------------

QString MuseScore::getSaveScoreName(const QString& title, QString& name,
                                    const QString& filter, bool selectFolder,
                                          QString* selectedFilter)
      {
      QFileInfo myName(name);
      if (myName.isRelative())
            myName.setFile(QDir::home(), name);
      name = myName.absoluteFilePath();

      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
            QFileDialog::Options options = selectFolder ? QFileDialog::ShowDirsOnly : QFileDialog::Options(0);
            return QFileDialog::getSaveFileName(this, title, name, filter, selectedFilter, options);
            }

      QFileInfo myScores(preferences.getString(PREF_APP_PATHS_MYSCORES));
      if (myScores.isRelative())
            myScores.setFile(QDir::home(), preferences.getString(PREF_APP_PATHS_MYSCORES));
      if (saveScoreDialog == 0) {
            saveScoreDialog = new QFileDialog(this);
            saveScoreDialog->setFileMode(QFileDialog::AnyFile);
            saveScoreDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
            saveScoreDialog->setOption(QFileDialog::DontUseNativeDialog, true);
            saveScoreDialog->setAcceptMode(QFileDialog::AcceptSave);
            addScorePreview(saveScoreDialog);

            restoreDialogState("saveScoreDialog", saveScoreDialog);
            }
      // setup side bar urls
      saveScoreDialog->setSidebarUrls(sidebarUrls());

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
      QFileInfo myStyles(preferences.getString(PREF_APP_PATHS_MYSTYLES));
      if (myStyles.isRelative())
            myStyles.setFile(QDir::home(), preferences.getString(PREF_APP_PATHS_MYSTYLES));
      QString defaultPath = myStyles.absoluteFilePath();

      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
            QString fn;
            if (open) {
                  fn = QFileDialog::getOpenFileName(
                     this, tr("Load Style"),
                     defaultPath,
                     tr("MuseScore Styles") + " (*.mss)"
                     );
                  }
            else {
                  fn = QFileDialog::getSaveFileName(
                     this, tr("Save Style"),
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
                  loadStyleDialog->setWindowTitle(title.isEmpty() ? tr("Load Style") : title);
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
                  saveStyleDialog->setWindowTitle(title.isEmpty() ? tr("Save Style") : title);
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

      QFileInfo myStyles(preferences.getString(PREF_APP_PATHS_MYSTYLES));
      if (myStyles.isRelative())
            myStyles.setFile(QDir::home(), preferences.getString(PREF_APP_PATHS_MYSTYLES));
      QString defaultPath = myStyles.absoluteFilePath();

      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
            QString fn;
            if (open) {
                  fn = QFileDialog::getOpenFileName(
                     this, tr("Load Chord Symbols Style"),
                     defaultPath,
                     filter
                     );
                  }
            else {
                  fn = QFileDialog::getSaveFileName(
                     this, tr("Save Chord Symbols Style"),
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

      QSettings set;
      if (open) {
            if (loadChordStyleDialog == 0) {
                  loadChordStyleDialog = new QFileDialog(this);
                  loadChordStyleDialog->setFileMode(QFileDialog::ExistingFile);
                  loadChordStyleDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  loadChordStyleDialog->setWindowTitle(tr("Load Chord Symbols Style"));
                  loadChordStyleDialog->setNameFilter(filter);
                  loadChordStyleDialog->setDirectory(defaultPath);

                  restoreDialogState("loadChordStyleDialog", loadChordStyleDialog);
                  loadChordStyleDialog->restoreState(set.value("loadChordStyleDialog").toByteArray());
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
                  saveChordStyleDialog->setWindowTitle(tr("Save Style"));
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
      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
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
            loadScanDialog->setWindowTitle(tr("Choose PDF Scan"));
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
      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
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
            loadAudioDialog->setWindowTitle(tr("Choose Ogg Audio File"));
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
      QString title = tr("Save Image");

      QFileInfo myImages(preferences.getString(PREF_APP_PATHS_MYIMAGES));
      if (myImages.isRelative())
            myImages.setFile(QDir::home(), preferences.getString(PREF_APP_PATHS_MYIMAGES));
      QString defaultPath = myImages.absoluteFilePath();

      // compute the image capture path
      QString myCapturePath;
      // if no saves were made for current score, then use the score's name as default
      if (!cs->masterScore()->savedCapture()) {
            // set the current score's name as the default name for saved captures
            QString scoreName = cs->masterScore()->fileInfo()->completeBaseName();
            QString name = createDefaultFileName(scoreName);
            QString fname = QString("%1/%2").arg(defaultPath).arg(name);
            QFileInfo myCapture(fname);
            if (myCapture.isRelative())
                myCapture.setFile(QDir::home(), fname);
            myCapturePath = myCapture.absoluteFilePath();
            }
      else
            myCapturePath = lastSaveCaptureName;

      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
            QString fn;
            fn = QFileDialog::getSaveFileName(
               this,
               title,
               myCapturePath,
               filter,
               selectedFilter
               );
            // if a save was made for this current score
            if (!fn.isEmpty()) {
                cs->masterScore()->setSavedCapture(true);
                // store the last name used for saving an image capture
                lastSaveCaptureName = fn;
                }
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

      // set file's name using the computed path
      saveImageDialog->selectFile(myCapturePath);

      if (saveImageDialog->exec()) {
            QStringList result = saveImageDialog->selectedFiles();
            *selectedFilter = saveImageDialog->selectedNameFilter();
            // a save was made for this current score
            cs->masterScore()->setSavedCapture(true);
            // store the last name used for saving an image capture
            lastSaveCaptureName = result.front();
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
            title  = tr("Load Palette");
            filter = tr("MuseScore Palette") + " (*.mpal)";
            }
      else {
            title  = tr("Save Palette");
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

      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
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
            title  = tr("Load Plugin");
            filter = tr("MuseScore Plugin") + " (*.qml)";
            }
      else {
            title  = tr("Save Plugin");
            filter = tr("MuseScore Plugin File") + " (*.qml)";
            }

      QFileInfo myPlugins(preferences.getString(PREF_APP_PATHS_MYPLUGINS));
      if (myPlugins.isRelative())
            myPlugins.setFile(QDir::home(), preferences.getString(PREF_APP_PATHS_MYPLUGINS));
      QString defaultPath = myPlugins.absoluteFilePath();

      QString name  = createDefaultFileName("Plugin");
      QString fname = QString("%1/%2.qml").arg(defaultPath).arg(name);
      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
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

                  QSettings set;
                  loadPluginDialog->restoreState(set.value("loadPluginDialog").toByteArray());
                  loadPluginDialog->setAcceptMode(QFileDialog::AcceptOpen);
                  }
            urls.append(QUrl::fromLocalFile(mscoreGlobalShare+"/plugins"));
            dialog = loadPluginDialog;
            }
      else {
            if (savePluginDialog == 0) {
                  savePluginDialog = new QFileDialog(this);
                  QSettings set;
                  savePluginDialog->restoreState(set.value("savePluginDialog").toByteArray());
                  savePluginDialog->setAcceptMode(QFileDialog::AcceptSave);
                  savePluginDialog->setFileMode(QFileDialog::AnyFile);
                  savePluginDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
                  savePluginDialog->setOption(QFileDialog::DontUseNativeDialog, true);
                  savePluginDialog->setWindowTitle(tr("Save Plugin"));
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
            title  = tr("Load Drumset");
            filter = tr("MuseScore Drumset") + " (*.drm)";
            }
      else {
            title  = tr("Save Drumset");
            filter = tr("MuseScore Drumset File") + " (*.drm)";
            }

      QFileInfo myStyles(preferences.getString(PREF_APP_PATHS_MYSTYLES));
      if (myStyles.isRelative())
            myStyles.setFile(QDir::home(), preferences.getString(PREF_APP_PATHS_MYSTYLES));
      QString defaultPath  = myStyles.absoluteFilePath();

      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
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
#ifndef QT_NO_PRINTER
      LayoutMode layoutMode = cs->layoutMode();
      if (layoutMode != LayoutMode::PAGE) {
            cs->setLayoutMode(LayoutMode::PAGE);
            cs->doLayout();
            }

      QPrinter printerDev(QPrinter::HighResolution);
      printerDev.setPageLayout(cs->style().pageOdd());
      printerDev.setCreator("MuseScore Version: " VERSION);
      printerDev.setFullPage(true);
      if (!printerDev.setPageMargins(QMarginsF()))
            qDebug("unable to clear printer margins");
      printerDev.setColorMode(QPrinter::Color);
      if (cs->isMaster())
            printerDev.setDocName(cs->masterScore()->fileInfo()->completeBaseName());
      else
            printerDev.setDocName(cs->excerpt()->title());
      printerDev.setOutputFormat(QPrinter::NativeFormat);
      int pages    = cs->pages().size();
      printerDev.setFromTo(1, pages);

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
      printerDev.setOutputFileName("");
#else
      // when setting this on windows platform, pd.exec() does not
      // show dialog
      if (cs->isMaster())
            printerDev.setOutputFileName(cs->masterScore()->fileInfo()->path() + "/" + cs->masterScore()->fileInfo()->completeBaseName() + ".pdf");
      else
            printerDev.setOutputFileName(cs->masterScore()->fileInfo()->path() + "/" + cs->excerpt()->title() + ".pdf");
#endif

      QPrintDialog pd(&printerDev, 0);

      if (pd.exec()) {
            QPainter p(&printerDev);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            double mag_ = printerDev.logicalDpiX() / DPI;

            double pr = MScore::pixelRatio;
            MScore::pixelRatio = 1.0 / mag_;
            p.scale(mag_, mag_);

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
            MScore::pixelRatio = pr;
            }

      if (layoutMode != cs->layoutMode()) {
            cs->setLayoutMode(layoutMode);
            cs->doLayout();
            }
#endif
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
      fl.append(tr("Scalable Vector Graphics") + " (*" + EXT_SVG + ")");
#ifdef HAS_AUDIOFILE
      fl.append(tr("Wave Audio") + " (*.wav)");
      fl.append(tr("FLAC Audio") + " (*.flac)");
      fl.append(tr("Ogg Vorbis Audio") + " (*.ogg)");
#endif
#ifdef USE_LAME
      fl.append(tr("MP3 Audio") + " (*.mp3)");
#endif
      fl.append(tr("Standard MIDI File") + " (*.mid)");
      fl.append(tr("Compressed MusicXML File") + " (*.mxl)");
      fl.append(tr("Uncompressed MusicXML File") + " (*.musicxml)");
      fl.append(tr("Uncompressed MuseScore File") + " (*.mscx)");
// SMAWS options
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_AUTO_OPEN).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_AUTO_ALL).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_MULTI).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_GRID).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_GRID_RULERS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_TABLES).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_FRETS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_LYRICS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_MIX_TREE).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_RULERS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_TOUR).arg(EXT_VTT));
// SMAWS end

      QString saveDialogTitle = tr("Export");

      QString saveDirectory;
      if (cs->masterScore()->fileInfo()->exists())
            saveDirectory = cs->masterScore()->fileInfo()->dir().path();
      else {
            QSettings set;
            if (lastSaveCopyDirectory.isEmpty())
                  lastSaveCopyDirectory = set.value("lastSaveCopyDirectory", preferences.getString(PREF_APP_PATHS_MYSCORES)).toString();
            saveDirectory = lastSaveCopyDirectory;
            }

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.getString(PREF_APP_PATHS_MYSCORES);

      if (lastSaveCopyFormat.isEmpty())
            lastSaveCopyFormat = settings.value("lastSaveCopyFormat", "pdf").toString();
      QString saveFormat = lastSaveCopyFormat;

      if (saveFormat.isEmpty())
            saveFormat = "pdf";

      QString name;
#ifdef Q_OS_WIN
      if (QSysInfo::WindowsVersion == QSysInfo::WV_XP) {
            if (!cs->isMaster())
                  name = QString("%1/%2-%3").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->title()));
            else
                  name = QString("%1/%2").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName());
            }
      else
#endif
      if (!cs->isMaster())
            name = QString("%1/%2-%3.%4").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->title())).arg(saveFormat);
      else
            name = QString("%1/%2.%3").arg(saveDirectory).arg(cs->masterScore()->fileInfo()->completeBaseName()).arg(saveFormat);

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

      if (fi.suffix().isEmpty())
            QMessageBox::critical(this, tr("Export"), tr("Cannot determine file type"));
      // SaveAs() is restrictive in a variety of ways, especially the lack of
      // selectedFilter. For SMAWS, this is all that's necessary:
      else if (fn.right(4) == EXT_VTT) {
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
            else if (selectedFilter.contains(FILTER_SMAWS_TOUR))
                  saveSMAWS_Tour(cs, &fi);
      }

      else // Everything NOT SMAWS
            saveAs(cs, true, fn, fi.suffix());
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
      fl.append(tr("Scalable Vector Graphics") + " (*" + EXT_SVG + ")");
#ifdef HAS_AUDIOFILE
      fl.append(tr("Wave Audio") + " (*.wav)");
      fl.append(tr("FLAC Audio") + " (*.flac)");
      fl.append(tr("Ogg Vorbis Audio") + " (*.ogg)");
#endif
#ifdef USE_LAME
      fl.append(tr("MP3 Audio") + " (*.mp3)");
#endif
      fl.append(tr("Standard MIDI File") + " (*.mid)");
      fl.append(tr("Compressed MusicXML File") + " (*.mxl)");
      fl.append(tr("Uncompressed MusicXML File") + " (*.musicxml)");
      fl.append(tr("MuseScore File") + " (*.mscz)");
      fl.append(tr("Uncompressed MuseScore File") + " (*.mscx)");

      QString saveDialogTitle = tr("Export Parts");

      QString saveDirectory;
      if (cs->masterScore()->fileInfo()->exists())
            saveDirectory = cs->masterScore()->fileInfo()->dir().path();
      else {
            QSettings set;
            if (lastSaveCopyDirectory.isEmpty())
                lastSaveCopyDirectory = set.value("lastSaveCopyDirectory", preferences.getString(PREF_APP_PATHS_MYSCORES)).toString();
            saveDirectory = lastSaveCopyDirectory;
            }

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.getString(PREF_APP_PATHS_MYSCORES);

      if (lastSaveCopyFormat.isEmpty())
            lastSaveCopyFormat = settings.value("lastSaveCopyFormat", "pdf").toString();
      QString saveFormat = lastSaveCopyFormat;

      if (saveFormat.isEmpty())
            saveFormat = "pdf";

      QString scoreName = cs->isMaster() ? cs->masterScore()->fileInfo()->completeBaseName() : cs->title();
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
            QMessageBox::critical(this, tr("Export Parts"), tr("Cannot determine file type"));
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
            QString partfn = fi.absolutePath() + "/" + fi.completeBaseName() + "-" + createDefaultFileName(pScore->title()) + "." + ext;
            QFileInfo fip(partfn);
            if (fip.exists() && !overwrite) {
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
            QString partfn(fi.absolutePath() + "/" + fi.completeBaseName() + "-" + createDefaultFileName(tr("Score_and_Parts")) + ".pdf");
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
            QMessageBox::information(this, tr("Export Parts"), tr("Parts were successfully exported"));
      return true;
      }

//---------------------------------------------------------
//   saveAs
//---------------------------------------------------------

bool MuseScore::saveAs(Score* cs_, bool saveCopy, const QString& path, const QString& ext)
      {
      bool rv = false;
      QString suffix = "." + ext;
      QString fn(path);
      if (!fn.endsWith(suffix))
            fn += suffix;

      LayoutMode layoutMode = cs_->layoutMode();
      if (ext == "mscx" || ext == "mscz") {
            // save as mscore *.msc[xz] file
            QFileInfo fi(fn);
            rv = true;
            // store new file and path into score fileInfo
            // to have it accessible to resources
            QFileInfo originalScoreFileInfo(*cs_->masterScore()->fileInfo());
            cs_->masterScore()->fileInfo()->setFile(fn);
            if (!cs_->isMaster()) { // clone metaTags from masterScore
                  QMapIterator<QString, QString> j(cs_->masterScore()->metaTags());
                  while (j.hasNext()) {
                        j.next();
                        if (j.key() != "partName") // don't copy "partName" should that exist in masterScore
                              cs_->metaTags().insert(j.key(), j.value());
#if defined(Q_OS_WIN)   // Update "platform", may not be worth the effort
                        cs_->metaTags().insert("platform", "Microsoft Windows");
#elif defined(Q_OS_MAC)
                        cs_->metaTags().insert("platform", "Apple Macintosh");
#elif defined(Q_OS_LINUX)
                        cs_->metaTags().insert("platform", "Linux");
#else
                        cs_->metaTags().insert("platform", "Unknown");
#endif
                        cs_->metaTags().insert("source", ""); // Empty "source" to avoid clashes with masterrScore when doing "Save online"
                        cs_->metaTags().insert("creationDate", QDate::currentDate().toString(Qt::ISODate)); // update "creationDate"
                        }
                  }
            try {
                  if (ext == "mscz")
                        cs_->saveCompressedFile(fi, false);
                  else
                        cs_->saveFile(fi);
                  }
            catch (QString s) {
                  rv = false;
                  QMessageBox::critical(this, tr("Save As"), s);
                  }
            if (!cs_->isMaster()) { // remove metaTags added above
                  QMapIterator<QString, QString> j(cs_->masterScore()->metaTags());
                  while (j.hasNext()) {
                        j.next();
                        // remove all but "partName", should that exist in masterScore
                        if (j.key() != "partName")
                              cs_->metaTags().remove(j.key());
                        }
                  }
            *cs_->masterScore()->fileInfo() = originalScoreFileInfo;          // restore original file info

            if (rv && !saveCopy) {
                  cs_->masterScore()->fileInfo()->setFile(fn);
                  updateWindowTitle(cs_);
                  cs_->undoStack()->setClean();
                  dirtyChanged(cs_);
                  cs_->setCreated(false);
                  scoreCmpTool->updateScoreVersions(cs_);
                  addRecentScore(cs_);
                  writeSessionFile(false);
                  }
            }
      else if (ext == "musicxml") {
            // save as MusicXML *.musicxml file
            rv = saveXml(cs_, fn);
            }
      else if (ext == "mxl") {
            // save as compressed MusicXML *.mxl file
            rv = saveMxl(cs_, fn);
            }
      else if (ext == "mid") {
            // save as midi file *.mid
            rv = saveMidi(cs_, fn);
            }
      else if (ext == "pdf") {
            // save as pdf file *.pdf
            cs_->switchToPageMode();
            rv = savePdf(cs_, fn);
            }
      else if (ext == "png") {
            // save as png file *.png
            cs_->switchToPageMode();
            rv = savePng(cs_, fn);
            }
      else if (ext == "svg") {
            // save as svg file *.svg
            cs_->switchToPageMode();
            rv = saveSvg(cs_, fn);
            }
#ifdef HAS_AUDIOFILE
      else if (ext == "wav" || ext == "flac" || ext == "ogg")
            rv = saveAudio(cs_, fn);
#endif
#ifdef USE_LAME
      else if (ext == "mp3")
            rv = saveMp3(cs_, fn);
#endif
      else if (ext == "spos") {
            cs_->switchToPageMode();
            // save positions of segments
            rv = savePositions(cs_, fn, true);
            }
      else if (ext == "mpos") {
            cs_->switchToPageMode();
            // save positions of measures
            rv = savePositions(cs_, fn, false);
            }
      else if (ext == "mlog") {
            rv = cs_->sanityCheck(fn);
            }
      else if (ext == "metajson") {
            rv = saveMetadataJSON(cs, fn);
            }
      else {
            qDebug("Internal error: unsupported extension <%s>",
               qPrintable(ext));
            return false;
            }
      if (!rv && !MScore::noGui)
            QMessageBox::critical(this, tr("MuseScore:"), tr("Cannot write into %1").arg(fn));

      if (layoutMode != cs_->layoutMode()) {
            cs_->setLayoutMode(layoutMode);
            cs_->doLayout();
            }
      return rv;
      }

//---------------------------------------------------------
//   saveMidi
//---------------------------------------------------------

bool MuseScore::saveMidi(Score* score, const QString& name)
      {
      ExportMidi em(score);
      return em.write(name, preferences.getBool(PREF_IO_MIDI_EXPANDREPEATS), preferences.getBool(PREF_IO_MIDI_EXPORTRPNS));
      }

bool MuseScore::saveMidi(Score* score, QIODevice* device)
      {
      ExportMidi em(score);
      return em.write(device, preferences.getBool(PREF_IO_MIDI_EXPANDREPEATS), preferences.getBool(PREF_IO_MIDI_EXPORTRPNS));
      }

//---------------------------------------------------------
//   savePdf
//---------------------------------------------------------

bool MuseScore::savePdf(const QString& saveName)
      {
      return savePdf(cs, saveName);
      }

bool MuseScore::savePdf(Score* score, const QString& saveName)
      {
      QPrinter printer;
      printer.setOutputFileName(saveName);
      return savePdf(score, printer);
      }

bool MuseScore::savePdf(Score* score, QPrinter& printer)
      {
      score->setPrinting(true);
      MScore::pdfPrinting = true;

      MPageLayout& odd = score->style().pageOdd();

      printer.setResolution(preferences.getInt(PREF_EXPORT_PDF_DPI));
      printer.setPageLayout(odd);
      printer.setFullPage(true);
      printer.setColorMode(QPrinter::Color);
#if defined(Q_OS_MAC)
      printer.setOutputFormat(QPrinter::NativeFormat);
#else
      printer.setOutputFormat(QPrinter::PdfFormat);
#endif

      printer.setCreator("MuseScore Version: " VERSION);
      if (!printer.setPageMargins(QMarginsF()))
            qDebug("unable to clear printer margins");

      QString title = score->metaTag("workTitle");
      if (title.isEmpty()) // workTitle unset?
            title = score->masterScore()->title(); // fall back to (master)score's tab title
      if (!score->isMaster()) { // excerpt?
            QString partname = score->metaTag("partName");
            if (partname.isEmpty()) // partName unset?
                  partname = score->title(); // fall back to excerpt's tab title
            title += " - " + partname;
            }
      printer.setDocName(title); // set PDF's meta data for Title

      QPainter p;
      if (!p.begin(&printer))
            return false;
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);

      QRectF rect = odd.fullRect(QPageLayout::Inch);
      p.setViewport(QRect(0, 0, rect.width()  * printer.logicalDpiX(),
                                rect.height() * printer.logicalDpiY()));
      p.setWindow(  QRect(0, 0, rect.width()  * DPI,
                                rect.height() * DPI));

      double pr = MScore::pixelRatio;
      MScore::pixelRatio = DPI / printer.logicalDpiX();

      const QList<Page*> pl = score->pages();
      int pages = pl.size();
      bool firstPage = true;
      for (int n = 0; n < pages; ++n) {
            if (!firstPage)
                  printer.newPage();
            firstPage = false;
            score->print(&p, n);
            }
      p.end();
      score->setPrinting(false);

      MScore::pixelRatio = pr;
      MScore::pdfPrinting = false;
      return true;
      }

bool MuseScore::savePdf(QList<Score*> scores, const QString& saveName)
      {
      if (scores.empty())
            return false;
      Score* firstScore = scores[0];
      QPageLayout& odd  = (QPageLayout&)(firstScore->style().pageOdd());

      QPdfWriter pdfWriter(saveName);
      pdfWriter.setResolution(preferences.getInt(PREF_EXPORT_PDF_DPI));
      pdfWriter.setPageLayout(odd);

      pdfWriter.setCreator("MuseScore Version: " VERSION);
      if (!pdfWriter.setPageMargins(QMarginsF()))
            qDebug("unable to clear printer margins");

      QString title = firstScore->metaTag("workTitle");
      if (title.isEmpty()) // workTitle unset?
            title = firstScore->title(); // fall back to (master)score's tab title
      title += " - " + tr("Score and Parts");
      pdfWriter.setTitle(title); // set PDF's meta data for Title

      QPainter p;
      if (!p.begin(&pdfWriter))
            return false;

      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);

      QRectF rect = odd.fullRect(QPageLayout::Inch);
      p.setViewport(QRect(0, 0, rect.width()  * pdfWriter.logicalDpiX(),
                                rect.height() * pdfWriter.logicalDpiY()));
      p.setWindow(  QRect(0, 0, rect.width()  * DPI,
                                rect.height() * DPI));


      double pr = MScore::pixelRatio;
      MScore::pixelRatio = DPI / pdfWriter.logicalDpiX();
      MScore::pdfPrinting = true;

      bool firstPage = true;
      for (Score* s : scores) {
            LayoutMode layoutMode = s->layoutMode();
            if (layoutMode != LayoutMode::PAGE) {
                  s->setLayoutMode(LayoutMode::PAGE);
            //      s->doLayout();
                  }
            s->doLayout();
            s->setPrinting(true);

            const QList<Page*> pl = s->pages();
            int pages    = pl.size();
            for (int n = 0; n < pages; ++n) {
                  if (!firstPage)
                        pdfWriter.newPage();
                  firstPage = false;
                  s->print(&p, n);
                  }
            //reset score
            s->setPrinting(false);

            if (layoutMode != s->layoutMode()) {
                  s->setLayoutMode(layoutMode);
                  s->doLayout();
                  }
            }
      p.end();
      MScore::pdfPrinting = false;
      MScore::pixelRatio = pr;
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
            QStringList pl = preferences.getString(PREF_APP_PATHS_MYSOUNDFONTS).split(";");
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
//   importExtension
//---------------------------------------------------------

void importExtension(QString name)
      {
      mscore->importExtension(name);
      }

//---------------------------------------------------------
//   readScore
///   Import file \a name
//---------------------------------------------------------

Score::FileError readScore(MasterScore* score, QString name, bool ignoreVersionError)
      {
      ScoreLoad sl;

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
      else if (suffix == "muxt") {
           importExtension(name);
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
                  { "musicxml", &importMusicXml       },
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
                  { "ptb",  &importGTP                },
                  };

            // import
            if (!preferences.getString(PREF_IMPORT_STYLE_STYLEFILE).isEmpty()) {
                  QFile f(preferences.getString(PREF_IMPORT_STYLE_STYLEFILE));
                  // silently ignore style file on error
                  if (f.open(QIODevice::ReadOnly))
                        score->style().load(&f);
                  }
            else {
                  if (score->styleB(Sid::chordsXmlFile))
                        score->style().chordList()->read("chords.xml");
                  score->style().chordList()->read(score->styleSt(Sid::chordDescriptionFile));
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

bool MuseScore::saveAs(Score* cs_, bool saveCopy)
      {
      QStringList fl;
      fl.append(tr("MuseScore File") + " (*.mscz)");
      fl.append(tr("Uncompressed MuseScore File") + " (*.mscx)");     // for debugging purposes
      QString saveDialogTitle = saveCopy ? tr("Save a Copy") :
                                           tr("Save As");

      QString saveDirectory;
      if (cs_->masterScore()->fileInfo()->exists())
            saveDirectory = cs_->masterScore()->fileInfo()->dir().path();
      else {
            QSettings set;
            if (saveCopy) {
                  if (mscore->lastSaveCopyDirectory.isEmpty())
                        mscore->lastSaveCopyDirectory = set.value("lastSaveCopyDirectory", preferences.getString(PREF_APP_PATHS_MYSCORES)).toString();
                  saveDirectory = mscore->lastSaveCopyDirectory;
                  }
            else {
                  if (mscore->lastSaveDirectory.isEmpty())
                        mscore->lastSaveDirectory = set.value("lastSaveDirectory", preferences.getString(PREF_APP_PATHS_MYSCORES)).toString();
                  saveDirectory = mscore->lastSaveDirectory;
                  }
            }

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.getString(PREF_APP_PATHS_MYSCORES);

      QString name;
#ifdef Q_OS_WIN
      if (QSysInfo::WindowsVersion == QSysInfo::WV_XP) {
            if (!cs_->isMaster())
                  name = QString("%1/%2-%3").arg(saveDirectory).arg(cs_->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->title()));
            else
                  name = QString("%1/%2").arg(saveDirectory).arg(cs_->masterScore()->fileInfo()->completeBaseName());
            }
      else
#endif
      if (!cs_->isMaster())
            name = QString("%1/%2-%3.mscz").arg(saveDirectory).arg(cs_->masterScore()->fileInfo()->completeBaseName()).arg(createDefaultFileName(cs->title()));
      else
            name = QString("%1/%2.mscz").arg(saveDirectory).arg(cs_->masterScore()->fileInfo()->completeBaseName());

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
                  QMessageBox::critical(mscore, tr("Save As"), tr("Cannot determine file type"));
            return false;
            }
      return saveAs(cs_, saveCopy, fn, fi.suffix());
      }

//---------------------------------------------------------
//   saveSelection
//    return true on success
//---------------------------------------------------------

bool MuseScore::saveSelection(Score* cs_)
      {
      if (!cs_->selection().isRange()) {
            if(!MScore::noGui) QMessageBox::warning(mscore, tr("Save Selection"), tr("Please select one or more measures"));
            return false;
            }
      QStringList fl;
      fl.append(tr("MuseScore File") + " (*.mscz)");
      QString saveDialogTitle = tr("Save Selection");

      QString saveDirectory;
      if (cs_->masterScore()->fileInfo()->exists())
            saveDirectory = cs_->masterScore()->fileInfo()->dir().path();
      else {
            QSettings set;
            if (mscore->lastSaveDirectory.isEmpty())
                  mscore->lastSaveDirectory = set.value("lastSaveDirectory", preferences.getString(PREF_APP_PATHS_MYSCORES)).toString();
            saveDirectory = mscore->lastSaveDirectory;
            }

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.getString(PREF_APP_PATHS_MYSCORES);

      QString name   = QString("%1/%2.mscz").arg(saveDirectory).arg(cs_->title());
      QString filter = fl.join(";;");
      QString fn     = mscore->getSaveScoreName(saveDialogTitle, name, filter);
      if (fn.isEmpty())
            return false;

      QFileInfo fi(fn);
      mscore->lastSaveDirectory = fi.absolutePath();

      QString ext = fi.suffix();
      if (ext.isEmpty()) {
            QMessageBox::critical(mscore, tr("Save Selection"), tr("Cannot determine file type"));
            return false;
            }
      bool rv = true;
      try {
            cs_->saveCompressedFile(fi, true);
            }
      catch (QString s) {
            rv = false;
            QMessageBox::critical(this, tr("Save Selected"), s);
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
         tr("Insert Image"),
         "",            // lastOpenPath,
         tr("All Supported Files") + " (*.svg *.jpg *.jpeg *.png);;" +
         tr("Scalable Vector Graphics") + " (*.svg);;" +
         tr("JPEG") + " (*.jpg *.jpeg);;" +
         tr("PNG Bitmap Graphic") + " (*.png)",
         0,
         preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS) ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog
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
//    return true on success.  Works with editor, shows additional windows.
//---------------------------------------------------------

bool MuseScore::savePng(Score* score, const QString& name)
      {
      const QList<Page*>& pl = score->pages();
      int pages = pl.size();
      int padding = QString("%1").arg(pages).size();
      bool overwrite = false;
      bool noToAll = false;
      for (int pageNumber = 0; pageNumber < pages; ++pageNumber) {
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
            QFile f(fileName);
            if (!f.open(QIODevice::WriteOnly))
                  return false;
            bool rv = savePng(score, &f, pageNumber);
            if (!rv)
                  return false;
            }
      return true;
      }

//---------------------------------------------------------
//   savePng with options
//    return true on success
//---------------------------------------------------------

bool MuseScore::savePng(Score* score, QIODevice* device, int pageNumber)
      {
      const bool screenshot = false;
      const bool transparent = preferences.getBool(PREF_EXPORT_PNG_USETRANSPARENCY);
      const double convDpi = preferences.getDouble(PREF_EXPORT_PNG_RESOLUTION);
      const int localTrimMargin = trimMargin;
      const QImage::Format format = QImage::Format_ARGB32_Premultiplied;

      bool rv = true;
      score->setPrinting(!screenshot);    // don’t print page break symbols etc.
      double pr = MScore::pixelRatio;

      QImage::Format f;
      if (format != QImage::Format_Indexed8)
          f = format;
      else
          f = QImage::Format_ARGB32_Premultiplied;

      const QList<Page*>& pl = score->pages();

      Page* page = pl.at(pageNumber);
      QRectF r;
      if (localTrimMargin >= 0) {
            QMarginsF margins(localTrimMargin, localTrimMargin, localTrimMargin, localTrimMargin);
            r = page->tbbox() + margins;
            }
      else
            r = page->abbox();
      int w = lrint(r.width()  * convDpi / DPI);
      int h = lrint(r.height() * convDpi / DPI);

      QImage printer(w, h, f);
      printer.setDotsPerMeterX(lrint((convDpi * 1000) / INCH));
      printer.setDotsPerMeterY(lrint((convDpi * 1000) / INCH));

      printer.fill(transparent ? 0 : 0xffffffff);
      double mag_ = convDpi / DPI;
      MScore::pixelRatio = 1.0 / mag_;

      QPainter p(&printer);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      p.scale(mag_, mag_);
      if (localTrimMargin >= 0)
            p.translate(-r.topLeft());
      QList< Element*> pel = page->elements();
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
      printer.save(device, "png");
      score->setPrinting(false);
      MScore::pixelRatio = pr;
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

      if (preferences.getBool(PREF_UI_APP_USENATIVEDIALOGS)) {
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

            QSettings set;
            loadBackgroundDialog->restoreState(set.value("loadBackgroundDialog").toByteArray());
            loadBackgroundDialog->setAcceptMode(QFileDialog::AcceptOpen);

            QSplitter* sp = loadBackgroundDialog->findChild<QSplitter*>("splitter");
            if (sp) {
                  WallpaperPreview* preview = new WallpaperPreview;
                  sp->addWidget(preview);
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

//---------------------------------------------------------
//   MuseScore::saveSvg
//---------------------------------------------------------
// [TODO:
// [In most of the functions above the Score* parameter is named "cs".
// [But there is also a member variable in class MuseScoreCore called "cs"
// [and it too is a Score*. If the member variable exists, the functions
// [should not bother to pass it around, especially with multiple names.
// [I have continued to use the "score" argument in this function, but
// [I was just following the existing convention, inconsistent as it is.
// [All the class MuseScore member functions should use the member variable.
// [This file is currently undergoing a bunch of changes, and that's the kind
// [of edit that must be coordinated with the MuseScore master code base.
//
bool MuseScore::saveSvg(Score* score, const QString& saveName)
      {
      const QList<Page*>& pl = score->pages();
      int pages = pl.size();
      int padding = QString("%1").arg(pages).size();
      bool overwrite = false;
      bool noToAll = false;
      for (int pageNumber = 0; pageNumber < pages; ++pageNumber) {
            QString fileName(saveName);
            if (fileName.endsWith(".svg"))
                  fileName = fileName.left(fileName.size() - 4);
            fileName += QString("-%1.svg").arg(pageNumber+1, padding, 10, QLatin1Char('0'));
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
            QFile f(fileName);
            if (!f.open(QIODevice::WriteOnly))
                  return false;
            bool rv = saveSvg(score, &f, pageNumber);
            if (!rv)
                  return false;
            }
      return true;
      }

//---------------------------------------------------------
//   MuseScore::saveSvg
///  Save a single page
//---------------------------------------------------------

bool MuseScore::saveSvg(Score* score, QIODevice* device, int pageNumber)
      {
      QString title(score->title());
      score->setPrinting(true);
      MScore::pdfPrinting = true;
      MScore::svgPrinting = true;
      const QList<Page*>& pl = score->pages();
      int pages = pl.size();
      double pr = MScore::pixelRatio;

      Page* page = pl.at(pageNumber);
      SvgGenerator printer;
      printer.setTitle(pages > 1 ? QString("%1 (%2)").arg(title).arg(pageNumber + 1) : title);
      printer.setOutputDevice(device);

      QRectF r;
      if (trimMargin >= 0) {
            QMarginsF margins(trimMargin, trimMargin, trimMargin, trimMargin);
            r = page->tbbox() + margins;
            }
      else
            r = page->abbox();
      qreal w = r.width();
      qreal h = r.height();
      printer.setSize(QSize(w, h));
      printer.setViewBox(QRectF(0, 0, w, h));
      QPainter p(&printer);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      if (trimMargin >= 0 && score->npages() == 1)
            p.translate(-r.topLeft());
      MScore::pixelRatio = DPI / printer.logicalDpiX();
      if (trimMargin >= 0)
             p.translate(-r.topLeft());
      // 1st pass: StaffLines
      for  (System* s : page->systems()) {
            for (int i = 0, n = s->staves()->size(); i < n; i++) {
                  if (score->staff(i)->invisible() || !score->staff(i)->show())
                        continue;  // ignore invisible staves
                  if (s->staves()->isEmpty() || !s->staff(i)->show())
                        continue;
                  Measure* fm = s->firstMeasure();
                  if (!fm) // only boxes, hence no staff lines
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
                  for (MeasureBase* mb = fm; mb; mb = s->nextMeasure(mb)) {
                        if (!mb->isMeasure() || !toMeasure(mb)->visible(i)) {
                              byMeasure = true;
                              break;
                              }
                        }
                  if (byMeasure) { // Draw visible staff lines by measure
                        for (MeasureBase* mb = fm; mb; mb = s->nextMeasure(mb)) {
                              if (mb->isMeasure() && toMeasure(mb)->visible(i)) {
                                    StaffLines* sl = toMeasure(mb)->staffLines(i);
                                    printer.setElement(sl);
                                    paintElement(p, sl);
                                    }
                              }
                        }
                  else { // Draw staff lines once per system
                        StaffLines* firstSL = s->firstMeasure()->staffLines(i)->clone();
                        StaffLines*  lastSL =  s->lastMeasure()->staffLines(i);

                        qreal lastX =  lastSL->bbox().right()
                                    +  lastSL->pagePos().x()
                                    - firstSL->pagePos().x();
                        QVector<QLineF>& lines = firstSL->getLines();
                        for (int l = 0, c = lines.size(); l < c; l++)
                              lines[l].setP2(QPointF(lastX, lines[l].p2().y()));

                        printer.setElement(firstSL);
                        paintElement(p, firstSL);
                        }
                  }
            }
      // 2nd pass: the rest of the elements
      QList<Element*> pel = page->elements();
      qStableSort(pel.begin(), pel.end(), elementLessThan);
      ElementType eType;
      for (const Element* e : pel) {
            // Always exclude invisible elements
            if (!e->visible())
                  continue;

            eType = e->type();
            switch (eType) { // In future sub-type code, this switch() grows, and eType gets used
            case ElementType::STAFF_LINES : // Handled in the 1st pass above
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
      p.end(); // Writes MuseScore SVG file to disk, finally

      // Clean up and return
      MScore::pixelRatio = pr;
      score->setPrinting(false);
      MScore::pdfPrinting = false;
      MScore::svgPrinting = false;
      return true;
      }

///////////////////////////////////////////////////////////////////////////////
// SMAWS functions:
//   svgInit()         initializes variables prior exporting a score
//   paintStaffLines() paints SVG staff lines more efficiently
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

    return QString("%1%2%3").arg(startTick, CUE_ID_FIELD_WIDTH, base, fillChar)
                            .arg(SMAWS_)
                            .arg(endTick,   CUE_ID_FIELD_WIDTH, base, fillChar);
}

// getAnnCueID()
// Gets the cue ID for an annotation, such as rehearsal mark or chord symbol,
// where the cue duration lasts until the next element of the same type.
static QString getAnnCueID(Score* score, const Element* e, EType eType)
{
    Segment* segStart = static_cast<Segment*>(e->parent());
    int     startTick = segStart->tick();

    for (Segment* seg = segStart->next1MM(SegmentType::ChordRest);
                  seg; seg = seg->next1MM(SegmentType::ChordRest)) {
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

//// Returns a full data-cue="cue1,cue2,..." string of comma-separated cue ids
//static QString getGrayOutCues(Score* score, int idxStaff, QStringList* pVTT)
//{
//    ///!!!For now these cues are not used in sheet music .svg files. The only
//    ///!!!gray-out cues are in the Mix Tree .vtt file.
//    return(QString(""));
//    ///!!!
//
//    int  startTick;
//    bool hasCues    = false;
//    bool isPrevRest = false;
//
//    QString     cue_id;
//    QString     cues;
//    QTextStream qts(&cues);
//
//    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasureMM())
//    {
//        // Empty measure = all rests
//        if (m->isMeasureRest(idxStaff)) {
//            if (!isPrevRest) {         // Start of gray-out cue
//                isPrevRest = true;
//                startTick  = m->tick();
//            }
//
//            if (!m->nextMeasureMM()) { // Final measure is empty
//                if (hasCues)
//                    qts << SVG_COMMA;
//                else  {
//                    qts << SVG_CUE;
//                    hasCues = true;
//                }
//
//                cue_id = getCueID(startTick, m->tick() + m->ticks());
//                qts << cue_id;
//                pVTT->append(cue_id);
//            }
//        }
//        else {
//            if (isPrevRest) {          // Complete any pending gray-out cue
//                if (hasCues)
//                    qts << SVG_COMMA;
//                else  {
//                    qts << SVG_CUE;
//                    hasCues = true;
//                }
//
//                cue_id = getCueID(startTick, m->tick());
//                qts << cue_id;
//                pVTT->append(cue_id);
//            }
//
//            isPrevRest = false;
//        }
//    }
//
//    if (hasCues)
//        qts << SVG_QUOTE;
//
//    return(cues);
//}

// Get the chord symbol, EType::HARMONY, for this segment
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

// Replaces non-CSSselector-compliant chars with a hyphen (aka dash)
// The QRegExp documentation does not include the use of \u, but it is the only
// way I could get this to work. \x is flawed inside [square brackets].
static QString stripNonCSS(const QString& str) {
    QString ret = str;
    QRegExp rx("[^A-Za-z0-9_\u00A0-\uFFFF]");
    return ret.replace(rx, "-");
}

// Converts non-ASCII chars to hex XML entities with leading zeros trimmed
static QString stringToUTF8(const QString& str, bool isTextContent = false) {
    QString ret;
    QChar   chr;
    for (int i = 0; i < str.size(); i++) {
        chr = str.at(i).unicode(); // VTT prohibits these chars in cue payload
        if (chr > 127 || (isTextContent && (chr == 38 || chr == 60 || chr == 62))) {
            ret.append(XML_ENTITY_BEGIN);
            ret.append(QString::number(str.at(i).unicode(), 16).toUpper());
            ret.append(XML_ENTITY_END);
        }
        else
            ret.append(str.at(i));
    }
    return ret;
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
                            QStringList*  pINames        =  0,
                            QStringList*  pFullNames     =  0,
                            QList<qreal>* pStaffTops     =  0)
{
    const qreal cursorOverlap = Ms::SPATIUM20 / 2; // half staff-space overlap, top + bottom

    bool  isMulti = (idxStaff != -1);
    bool  isFirst = true;             // first system
    bool  isStaff = true;
    qreal cursorTop;
    qreal cursorBot;
    qreal vSpacerUp = 0;
    qreal bot = -1;
    qreal top = -1;
    qreal y   =  0;

    // isMulti requires a <g></g> wrapper around each staff's elements
    if (isMulti && idxStaff > -1 && pINames != 0  && pVisibleStaves != 0) {
        Staff*          staff = score->staves()[idxStaff];
        QList<Staff*>* staves = staff->part()->staves();
        int              size = staves->size();
        bool           isLink = staff->links();
        bool         isSingle = (size == 1);

        // Multi-staff "parts" that are not linked (e.g. piano)
        isStaff = (isLink || isSingle || (*staves)[0]->idx() == idxStaff);
        if (isStaff) {
            bool isTab = staff->isTabStaff(0);

            const int gridHeight = 30;
            const int tabHeight  = 53;
            const int stdHeight  = 45;                //!!! see below
            const int idxVisible = (*pVisibleStaves)[idxStaff];

            System* s = page->systems().value(0);
            top = s->staff(idxStaff)->y();

            if (s->firstMeasure()->vspacerUp(idxStaff) != 0) {
                // This measure claims the extra space between it and the staff above it
                // Get the previous visible staff - top staff can't have a vertical spacer up
                for (int i = idxStaff - 1; i >= 0; i--) {
                    if ((*pVisibleStaves)[i] > 0) {
                        vSpacerUp = top - (s->staff(i)->y() + (score->staff(i)->isTabStaff(0) ? tabHeight : stdHeight));
                        break;
                    }
                }
            }

            // > 0 when multi-staff part that is not linked staves, e.g. piano.
            int lastStaff = (isSingle || isLink ? -1 : (*staves)[size - 1]->idx());

            if (idxVisible >= 0 && idxVisible < nVisible - 1) {
                // Get the next visible staff (below)
                for (int i = idxStaff + 1; i < pVisibleStaves->size(); i++) {
                    if ((*pVisibleStaves)[i] > 0) {
                        if (s->firstMeasure()->vspacerUp(i) != 0)
                            // Next staff (below) claims the extra space below this staff
                            bot = top + (isTab ? tabHeight : stdHeight); //!!! I assume that this staff's height is the standard 45pt/px or 53 for tablature
                        else if (i <= lastStaff)
                            continue;
                        else
                            bot = s->staff(i)->y(); // top of next visible staff
                        break;
                    }
                }
            }

            top -= vSpacerUp;
            if (bot < 0)
                bot = page->height() - pStaffTops->value(0) - page->bm();

            // Round these for crisp 2px lines
            y   = top * -1;   // for setYOffset()
            top = qRound(top);
            bot = qRound(bot);

            // Standard notation, tablature, or grid? I need to know by staff.
            // For Multi, the short name is the long name and vice versa, the
            // short name never appears in the sheet music: only one system.
            // The short name is the longer name in the staves list. For linked
            // tabs staves, the shortName is the note staff's long name.
            // JavaScript uses this to link the staves, which are separate in SVG.
            QString qs = staff->part()->longName();
            const QString shortName = (isTab && isLink
                                     ? stringToUTF8(stripNonCSS(staff->part()->longName()))
                                     : stringToUTF8(staff->part()->shortName(0), true));
            const bool    isGrid    = (shortName == STAFF_GRID);
            const int     height    = (isGrid ? gridHeight : bot - top);
            const QString className = (isGrid ? CLASS_GRID
                                              : (isTab ? CLASS_TABS
                                                       : CLASS_NOTES));
            if (isTab && isLink)
                qs.append("Tabs");
            pINames->append(stringToUTF8(stripNonCSS(qs)));
            pFullNames->append(shortName);
            printer->beginMultiGroup(pINames, pFullNames, className, height, top);
            printer->setCueID("");
        }
    } // if (isMulti)

    bool isVertical = printer->isScrollVertical();
    if (isVertical) { // at this point it's the same as !_isMulti...
        printer->setStaffLines(score->staves()[0]->lines(0));
        printer->beginGroup();
    }

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
                if (isFirst) {
                    int j;
                    StaffLines* sl = s->firstMeasure()->staffLines(i);

                    if (top < 0) // !isMulti
                        top = sl->bbox().top() + sl->pagePos().y();

                    // Get the first visible staff index (in the first system)
                    for (j = 0; j < pVisibleStaves->size(); j++) {
                        if (pVisibleStaves->value(j) >= 0)
                            break;
                    }

                    if (i == j) { // only first visible staff in first system
                        // Set the cursor's y-coord
                        cursorTop = (isMulti ? 5 : top - cursorOverlap);
                        printer->setCursorTop(cursorTop);

                        if (!isMulti) { // multi has fixed margins, only one system
                            // Get the left and right edges of the staff lines for
                            // title, subtitle, composer, and poet layout
                            StaffLines* ls =  s->lastMeasure()->staffLines(i);
                            qreal right = ls->bbox().right()
                                        + ls->pagePos().x()
                                        - sl->pagePos().x();
                            printer->setLeftRight(sl->bbox().left(), right);

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
                    }

                    if (isMulti && isStaff && pStaffTops != 0) {
                        // Offset between this staff and the first visible staff
                        pStaffTops->append(top);
                        printer->setYOffset(y);
                    }
                }
            }

            // Ignore invisible elements
            if (score->staff(i)->invisible() || !score->staff(i)->show())
                continue;
            if (s->staves()->isEmpty() || !s->staff(i)->show())
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
            MeasureBase* mb;
            QString cue_id;
            for (mb = s->firstMeasure(); mb != 0; mb = s->nextMeasure(mb)) {
                  if (mb->type() == EType::HBOX
                   || mb->type() == EType::VBOX
                   || !static_cast<Measure*>(mb)->visible(i)) {
                        byMeasure = true;
                        break;
                  }
            }
            if (byMeasure) { // Draw visible staff lines by measure
                  for (mb = s->firstMeasure(); mb != 0; mb = s->nextMeasure(mb)) {
                        if (mb->type() != EType::HBOX
                         && mb->type() != EType::VBOX
                         && static_cast<Measure*>(mb)->visible(i))
                        {
                              if (isVertical && i == 0)
                                  cue_id = getCueID(s->firstMeasure()->tick());
                              else
                                  cue_id = "";
                              printer->setCueID(cue_id);

                              StaffLines* sl = static_cast<Measure*>(mb)->staffLines(i);
                              printer->setElement(sl);
                              paintElement(*p, sl);
                        }
                  }
            }
            else { // Draw staff lines once per system
                StaffLines* firstSL = s->firstMeasure()->staffLines(i)->clone();
                StaffLines*  lastSL =  s->lastMeasure()->staffLines(i);
                qreal lastX = lastSL->bbox().right()
                            + lastSL->pagePos().x()
                            - firstSL->pagePos().x();

                QVector<QLineF>& lines = firstSL->getLines();
                for (int l = 0, c = lines.size(); l < c; l++)
                lines[l].setP2(QPointF(lastX, lines[l].p2().y()));

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
        if (s->staves()->size() > 0) //!!invisible first systems in parts...
            isFirst = false;

    } //for each System
    if (isVertical)
        printer->endGroup();
}

// svgInit() - consolidates shared code in saveSVG and saveSMAWS.
//             for MuseScore master, no harm, no gain, 100% neutral
static bool svgInit(Score*        score,
              const QString&      saveName,
                    SvgGenerator* printer,
                    QPainter*     p,
                    Page*         page = 0)
{
    printer->setFileName(saveName);
    printer->setTitle(score->metaTag("workTitle"));
    score->setPrinting(true);
    MScore::pdfPrinting = true;
    MScore::svgPrinting = true;
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    QRectF r;
    if (!page)
        page = score->pages().first();
    if (trimMargin >= 0) {
          QMarginsF margins(trimMargin, trimMargin, trimMargin, trimMargin);
          r = page->tbbox() + margins;
          }
    else
          r = page->abbox();
    qreal w = r.width();
    qreal h = r.height();
    if (trimMargin >= 0 && score->npages() == 1)
          p->translate(-r.topLeft());

    // The relationship between the viewBox dimensions and the width/height
    // values, combined with the preserveAspectRatio value, determine default
    // scaling inside the SVG file. (preserveAspectRatio="xMinYMin slice")
    printer->setViewBox(QRectF(0, 0, w, h));
    printer->setSize(QSize(w, h));
    if (!p->begin(printer))
          return false;
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
    return QTime::fromMSecsSinceStartOfDay(ticks2msecs(ticks, tempos))
                 .toString("hh:mm:ss.zzz");
}

// Start-time-only cues. Currently only Fretboards use them.
// Return the cue's first two lines: 123 (not fixed width)
//                                   00:00:00.000 --> 00:00:00.001
static QString getVTTStartCue(int tick, const TempoMap* tempos) {
    int msecs = ticks2msecs(tick, tempos);
    return QString(VTT_CUE_3_ARGS)
               .arg(QString::number(tick))
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
                                  .arg(QTime::fromMSecsSinceStartOfDay(endTime)
                                             .toString("hh:mm:ss.zzz"));
}

// This gets used a couple/few times
static QString smawsDesc(Score* score) {
    return QString(SMAWS_DESC_STUB).arg(score->metaTag("copyright"))
                                   .arg(score->metaTag("composer"))
                                   .arg(VERSION)
                                   .arg(SMAWS_VERSION);
}

// Paints the animated elements specified in the CueMultis
static void paintStaffSMAWS(Score*        score,
                            QPainter*     p,
                            SvgGenerator* printer,
                            CueMap*       barLines,
                            CueMulti*     mapFrozen,
                            CueMulti*     mapSVG,
                            CueMulti*     mapLyrics,
                            QVector<int>* pVisibleStaves =  0,
                            QList<qreal>* pStaffTops     =  0,
                            int           idxStaff = -1,
                            int           lyricsHeight = -1)
{
    QString cue_id;
    bool isMouse = false;
    bool isMulti = (idxStaff != -1);
    int  idx = (isMulti ? pVisibleStaves->value(idxStaff) : idxStaff);

    // 2nd pass: Elements with cue_ids, sorted in their QMaps
    // BarLines first, but only for the first staff
    if (barLines != 0 && idx < 1) {
        printer->beginGroup(2);
        for (CueMap::iterator c = barLines->begin(); c != barLines->end(); ++c) {
            printer->setCueID(c.key());
            const Element* e = c.value();
            if (!isMulti)
                printer->setStaffIndex(pVisibleStaves->value(e->staffIdx()));
            printer->setElement(e);
            paintElement(*p, e);
        }
        printer->endGroup(2); // barLines group inside staff group
    }

    // Frozen pane elements, if there are any
    if (mapFrozen->size() > 0) {
        // Iterate by key, then by value in reverse order. This recreates the
        // MuseScore draw order within the key/cue_id. This is required for
        // the frozen pane to generate properly.
        printer->beginGroup(2);
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
            printer->freezeIt(idx);
        }
        printer->endGroup(2); // mapFrozen group inside staff group
    }

    // mapSVG (in reverse draw order, not a problem for these element types)
    printer->beginMouseGroup(); // animated elements are clickable
    CueMulti::iterator i;
    for (i = mapSVG->begin(); i != mapSVG->end(); ++i) {
        cue_id = i.key();
        printer->setCueID(cue_id);
        printer->setElement(i.value());
        paintElement(*p, i.value());
    }
    printer->endGroup(isMulti ? 2 : 0);     // mouse group inside staff group

    if (isMulti) {
        // Close any pending Multi-Select Staves group element
        Staff* staff = score->staff(idxStaff);
        QList<Staff*>* staves = staff->part()->staves();
        int size = staves->size();
        if (size == 1 || staff->links()
         || (*staves)[size - 1]->idx() == idxStaff)
            printer->endGroup(1); // ends the staff group

        // Lyrics are a separate <g> element pseudo staff
        if (mapLyrics->size() > 0) {
            printer->beginMultiGroup(0, 0, CLASS_LYRICS, lyricsHeight, pStaffTops->last());
            for (i = mapLyrics->begin(); i != mapLyrics->end(); ++i) {
                cue_id = i.key();
                if (!isMouse && !cue_id.isEmpty()) {
                    isMouse = true;
                    printer->beginMouseGroup();
                }
                printer->setCueID(cue_id);
///!!!OBSOLETE!!!                printer->setStartMSecs(startMSecsFromCueID(score, cue_id));
                printer->setElement(i.value());
                paintElement(*p, i.value());
            }
            printer->endGroup(2); // mouse group
            printer->endGroup(1); // staff group
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
                          const int      precision,
                          const int      maxDigits,
                          const bool     withQuotes)
{
    QString qsReal = QString::number(n, 'f', precision);
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
static bool saveStartVTT(Score*      score,
                   const QString&    fileRoot,
                         IntSet*     setVTT,
                         Int2StrMap* mapVTT)
{
    // Open a stream into the file
    QFile fileVTT;
    fileVTT.setFileName(QString("%1%2").arg(fileRoot).arg(EXT_VTT));
    fileVTT.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    QTextStream streamVTT(&fileVTT);
    streamVTT << VTT_START_ONLY; // stream the header

    // For translating MIDI ticks into milliseconds
    const TempoMap* tempos = score->tempomap();


    if (setVTT) { // setVTT is an std::set, sorted and everything
        for (IntSet::iterator i = setVTT->begin(); i != setVTT->end(); ++i)
            streamVTT << getVTTStartCue(*i, tempos) << endl;
    }
    else if (mapVTT) {
        for (Int2StrMap::iterator i = mapVTT->begin(); i != mapVTT->end(); ++i)
            streamVTT << getVTTStartCue(i.key(), tempos) << i.value() << endl << endl;
    }
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
    streamVTT << VTT_HEADER;        // stream the header

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

static bool saveMixedVTT (Score* score, const QString& fileRoot, IntPairSet* setVTT) {
    const TempoMap* tempos = score->tempomap();

    QFile fileVTT;
    fileVTT.setFileName(QString("%1%2").arg(fileRoot).arg(EXT_VTT));
    fileVTT.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    QTextStream streamVTT(&fileVTT); // open a stream into the file
    streamVTT << VTT_MIXED;          // stream the header

    for (auto i = setVTT->begin(); i != setVTT->end(); ++i) {
        if (i->second == -1)
            streamVTT << getVTTStartCue(i->first, tempos) << endl;
        else
            streamVTT << getVTTCueTwo(getCueID(i->first, i->second), tempos)
                      << endl;
    }

    streamVTT.flush();               // write and close the VTT file
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
    if (!svgInit(score, QString("%1%2").arg(fnRoot).arg(EXT_SVG), &printer, &p, 0))
        return false;

    // Custom SMAWS header, including proper reference to MuseScore
    printer.setDescription(smawsDesc(score));

    // The link between an SVG elements and a VTT cue. See getCueID().
    QString cue_id;

    // QSet is unordered, QStringList::removeDuplicates() creates a unique set.
    // setVTT     - a chronologically sorted set of unique cue_ids
    QStringList setVTT;

    // mapSVG      - a QMultiMap: key = cue_id; value = list of elements.
    // mapFrozen   - ditto, value = SCORE frozen pane elements
    // mapSysStaff - ditto, value = SCORE "system" staff: tempos, chords, markers (should be measure numbers!!!)
    // mapLyrics   - ditto, value = SCORE lyrics as a separate group
    CueMulti mapSVG;
    CueMulti mapFrozen;
    CueMulti mapSysStaff;
    CueMulti mapLyrics;
    CueMap   barLines; // not a multi-map

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
    const bool isScrollVertical = score->style().value(Sid::pageTwosided).toBool();
    printer.setScrollVertical(isScrollVertical);

    // visibleStaves - Frozen Panes and Multi-Select Staves deal with visible
    // staves only. This vector is the same size as score->nstaves(). If a
    // staff is invisible its value in the vector is -1, else it contains the
    // visible-staff index for that staff.
    IntVect visibleStaves;
    visibleStaves.resize(score->nstaves());
    int nVisible = 0;

    // nonStdStaves - Tablature and percussion staves require special treatment
    // in the frozen pane. They don't have keysigs, and the clef never changes,
    // but the timesig needs to be properly aligned with the other staves. The
    // slashes-only "grid" staff behaves this same way - it should be created
    // as a percussion staff, that will group it into this vector and not make
    // it a non-standard height like tablature staves.
    IntVect nonStdStaves;
    bool hasTabs = false; // for auto-export of fretboards

    // Lyrics staves. Managing preset staff height for the lyrics pseudo-staves
    // is dicey. It only affects the display of lyrics w/o notes. This is so
    // that the last (lowest) lyrics staff has an extra 10 pixels of height.
    int idxLastLyrics = -1;

    Segment*    seg;
    SegmentType st;
    Element*    clef;
    int         i, n;

    printer.frozenClefs(0, false);
    n = score->nstaves();
    for (i = 0; i < n; i++) {
        Staff* staff = score->staff(i);
        Part*  part  = staff->part();
        int    track = i * VOICES;

        // Visible staves, which is the SVG staves, including multi-staff parts
        if (!part->show()) {
            visibleStaves[i] = -1;
            continue;
        }
        visibleStaves[i] = nVisible;

        QList<Staff*>* staves = part->staves();
        int size = staves->size();
        if (size == 1 || (*staves)[size - 1]->idx() == i || staff->links())
            ++nVisible;

        // Non-standard staves
        if (staff->isDrumStaff(0) || staff->isTabStaff(0)) {
            nonStdStaves.push_back(i);
            if (isMulti && staff->isTabStaff(0))
                hasTabs = true; // for auto-export of fretboards later
        }

        st = SegmentType::ChordRest;    // last lyrics staff //!!bool Staff::hasLyrics() would be a good thing
        for (seg = score->firstMeasureMM()->first(st); seg; seg = seg->next1MM(st)) {
            ChordRest* cr = seg->cr(track);
            if (cr && !cr->lyrics().empty()) {
                idxLastLyrics = i;
                break;
            }
        }
        st   = SegmentType::HeaderClef; // for keysig/timesig x offset
        clef = score->firstMeasureMM()->first(st)->element(track);
        if (clef)
            printer.frozenClefs(0,
                                toClef(clef)->clefType() > ClefType::G_1);

// PARTIAL SOLUTION:
//        ClefList& clefList = staff->clefList();
//        for (auto cl = clefList.begin(); cl != clefList.end(); ++cl)
//            printer.frozenClefs(cl->first,
//                                cl->second._concertClef  > ClefType::G_1);
    }
    printer.setNStaves(nVisible);
    printer.setNonStandardStaves(&nonStdStaves);

    // It's too counter-intuitive to do this inside the staves loop w/ClefList
    int tick;
    st = SegmentType::Clef; // for keysig/timesig x offset
    for (seg = score->firstMeasureMM()->first(st); seg; seg = seg->next1MM(st)) {
        tick = seg->tick();
        printer.frozenClefs(tick, false);
        for (i = 0; i < n; i++) {
            if (visibleStaves[i] == -1)
                continue;
            clef = seg->element(i * VOICES);
            if (clef && toClef(clef)->clefType() > ClefType::G_1) {
                printer.frozenClefs(tick, true);
                break;
            }
        }
    }

    // The sort order for elmPtrs is critical: if (isMulti) by type, by staff;
    //                                         else         by type;
    QList<Element*> elmPtrs = page->elements();
    std::stable_sort(elmPtrs.begin(), elmPtrs.end(), elementLessThan);
    if (isMulti)
        std::stable_sort(elmPtrs.begin(), elmPtrs.end(), elementLessThanByStaff);
    else // Paint staff lines once, prior to painting anything else
        paintStaffLines(score, &p, &printer, page, &visibleStaves);

    QStringList      iNames;    // Only used by Multi-Select Staves.
    QStringList      fullNames; // Only used by Multi-Select Staves.
    QList<qreal>     staffTops; // ditto
    int              idxStaff;  // Everything is grouped by staff.
    EType            eType;     // Everything is determined by element type.
    const ChordRest* cr;        // It has start and end ticks for highlighting.

    int maxNote = 0; // data-maxnote = Max note duration for this score. Helps optimize
                     // highlighting previous notes when user changes start time.
                     ///!!!OBSOLETE!!!
    idxStaff = -1;
//    Beam* b;
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
                paintStaffSMAWS(score, &p, &printer, &barLines, &mapFrozen, &mapSVG,
                     &mapLyrics, &visibleStaves, &staffTops, idxStaff, lyricsHeight);
                mapFrozen.clear();
                mapLyrics.clear();
                mapSVG.clear();
            }
            // We're starting s new staff, paint its staff lines
            paintStaffLines(score, &p, &printer, page, &visibleStaves, nVisible,
                            idx, &iNames, &fullNames, &staffTops);
            idxStaff = idx;
        }

        // Paint inanimate elements and collect animation elements.
        cr = 0;
        eType = e->type();
        switch (eType) {
        case EType::STAFF_LINES :       /// Not animated, but handled previously.
            continue;
            break;
                                        /// Highlighted Elements
        case EType::REST       : //                = ChordRest subclass Rest
        case EType::LYRICS     : //        .parent = ChordRest
        case EType::NOTE       : //        .parent = ChordRest
        case EType::NOTEDOT    : // .parent.parent = ChordRest subclass Chord
        case EType::ACCIDENTAL : // .parent.parent = ChordRest subclass Chord
//        case EType::STEM       : //         .chord = ChordRest subclass Chord
//        case EType::HOOK       : //         .chord = ChordRest subclass Chord
//        case EType::BEAM       : //      .elements = ChordRest vector
        case EType::HARMONY    : //     annotation = handled by getAnnCueId()
            switch (eType) {
            case EType::REST :
                cr = static_cast<const ChordRest*>(e);
                break;
            case EType::LYRICS :
            case EType::NOTE   :
                cr = static_cast<const ChordRest*>(e->parent());
                maxNote = qMax(maxNote, cr->actualTicks());
                break;
            case EType::NOTEDOT    :
				if (e->parent()->isRest()) {
					cr = static_cast<const ChordRest*>(e->parent());
					break;
				}                // else falls through
			case EType::ACCIDENTAL :
                cr = static_cast<const ChordRest*>(e->parent()->parent());
                break;
//            case EType::STEM :
//                cr = static_cast<const ChordRest*>(static_cast<const Stem*>(e)->chord());
//                break;
//            case EType::HOOK :
//                cr = static_cast<const ChordRest*>(static_cast<const Hook*>(e)->chord());
//                break;
//            case EType::BEAM : // special case: end tick is last note beamed
//                //This is because a const Beam* cannot call ->elements()!!!
//                b = static_cast<const Beam*>(e)->clone();
//                // Beam cue runs from first element to last
//                cue_id = getCueID(b->elements()[0]->tick(),
//                                  b->elements()[b->elements().size() - 1]->tick());
//                break;
            case EType::HARMONY : // special case: end tick is next HARMONY
                cue_id = getAnnCueID(score, e, eType);
                break;
            default:
                break; // should never happen
            }
            if (cr != 0) // exclude special cases
                cue_id = getCueID(cr->tick(), cr->tick() + cr->actualTicks());

            setVTT.append(cue_id);
            if (isMulti) {
                if(eType == EType::HARMONY)
                    mapSysStaff.insert(cue_id, e);  // "system" staff
                else if(eType == EType::LYRICS)
                    mapLyrics.insert(cue_id, e); // lyrics get their own pseudo-staff
                else
                    mapSVG.insert(cue_id, e);
            }
            else
                mapSVG.insert(cue_id, e);
            continue;

        case EType::BAR_LINE       :    /// Ruler/Scrolling elements
            // Add the cue ID to the VTT set, but don't add to mapSVG
            if (isMulti && visibleStaves[idxStaff] == 0
             && static_cast<const BarLine*>(e)->barLineType() == BLType::NORMAL) {
                cue_id = getScrollCueID(score, e);
                setVTT.append(cue_id);
                barLines.insert(cue_id, e);
                continue;
            }
            cue_id = "";
            break;

        case EType::REHEARSAL_MARK :
            if (isMulti) {
                mapSysStaff.insert("", e);
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
            Tid ss = Tid(static_cast<const Text*>(e)->subtype());
            if (isMulti && (ss == Tid::MEASURE_NUMBER
                         || ss == Tid::SYSTEM)) {
                mapSysStaff.insert("", e); // add to "system" staff, but no cue
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
        paintStaffSMAWS(score, &p, &printer, 0, &mapFrozen, &mapSVG, &mapLyrics,
                        &visibleStaves, &staffTops, idxStaff);

        // Paint the staff-independent elements
        // No staff lines, no bar lines, so no call to paintStaffLines()
        const QString system = "system";
        iNames.append(system);
        printer.setStaffIndex(nVisible); // only affects fancy formatting
        printer.setYOffset(0);
        printer.beginMultiGroup(&iNames, 0, system, 35, 0); ///!!! 35 is standard top staff line y-coord, I'm being lazy here by hardcoding it
        bool isMouse = false;
        for (CueMulti::iterator i = mapSysStaff.begin(); i != mapSysStaff.end(); ++i) {
            cue_id = i.key();
            if (!isMouse && !cue_id.isEmpty()) {
                isMouse = true;
                printer.beginMouseGroup();
            }
///!!!OBSOLETE!!!            printer.setStartMSecs(startMSecsFromCueID(score, cue_id));
            printer.setCueID(cue_id);
            printer.setElement(i.value());
            paintElement(p, i.value());
        }
        printer.endGroup(2); // mouse group
        printer.endGroup(1); // staff group

        // Multi-Select Staves frozen pane has <use> elements, one per staff
        staffTops.append(staffTops[0]); // For the staff-independent elements
        for (i = 0; i < iNames.size(); i++)
            printer.createMultiUse(staffTops[i]);
    }
    else {
        // Paint everything all at once, not by staff
        paintStaffSMAWS(score, &p, &printer, 0, &mapFrozen, &mapSVG, &mapLyrics, &visibleStaves);

//!!        // These go in the <svg> element for the full score/part
//!!        printer.setCueID(getGrayOutCues(score, -1, &setVTT));
    }

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

static void getRulersTemplate(QString* pqs, const QString& fn, QFileInfo* qfi)
{
    QFile       qf;
    QTextStream qts;
    QTextStream qtsFile;

    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(fn));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qtsFile.setDevice(&qf);
    qts.setString(pqs);
    qts << qtsFile.readAll();
    qf.close();
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
                         QTextStream*   qts,
                         IntSet*        setVTT,
                         int            width)
{
    const int margin = 8; // margin on both sides of the ruler
    const int border = 2; // stroke-width of borders
    const int endX   = width - margin - border;

    // SVG values and partial values for various attributes
    QString lineID;
    QString textClass;
    const QString line1     = "line1\"   ";
    const QString line5     = "line5\"   ";
    const QString line10    = "line10\"  ";
    const QString lineMrks  = "lineMrks\"";
    const QString idBars    = "bars";
    const QString idMarkers = "mrks";

    const int xDigits = 4; // x-coord never greater than 9999

    int   tick;
    int   iBarNo;   // Integer version of measure number
    int   pxX;      // x coordinates for lines are int
    qreal offX;     // x offset for rehearsal mark text

    // For the invisible, but clickable, rects around lines
    qreal rectX = 0;
    qreal rectWidth;
    qreal lineX; // The previous bar ruler line x-coord

    QString rectB;
    QString rectM;
    QString textB;
    QString textM;
    QString elm;
    QString label;    // <text> element contents
    QString noEvents; // elements with pointer-events="none"
    QString bars;     // markers elements must follow bars elements so that...
    QString marks;    // ...they can be on top of the full-height bar <rect>s
    QString style;    // the markers text and lines that javascript styles
    QString tempos;   // tempos separate for readability and consistency
    QTextStream qtsBars( &bars);
    QTextStream qtsMarks(&marks);
    QTextStream qtsStyle(&style);
    QTextStream qtsNoEvt(&noEvents);
    QTextStream qtsTempo(&tempos);
    QTextStream qtsFile;
    QTextStream* pqts;
    QFile       qf;
    bool isMarker;
    bool isTempo;

    qreal     x;
    Element*  e;
    EType     eType;
    int rectCue; // bars ruler rects apply to the previous bar
    int prevCue;

    // A multimap because a barline and a marker can share the same tick
    QMultiMap<int, Element*> mapSVG;
    QMultiMap<int, Element*>::iterator i;

    // Score::duration() returns # of seconds as an int, I need more accuracy
    const qreal duration = score->tempomap()->tick2time(score->lastMeasure()->tick()
                                                      + score->lastMeasure()->ticks());
    // Pixels of width per millisecond, left + right margins, right border
    const qreal pxPerMSec = (width - (margin * 2) - border) / (duration * 1000);

    // End of music tick is required in VTT for endRect and loopEnd
    tick = score->lastSegment()->tick();
    setVTT->insert(tick);

    // For fancy formatting
    const int cueIdDigits = QString::number(tick).size();
    const int barNoDigits = QString::number(score->lastMeasure()->no()).size();

    // Collect the ruler elements
    Measure* m;
    Segment* s;
    int      c;
    for (m = score->firstMeasure(); m; m = m->nextMeasureMM()) {
        // Markers ruler by Rehearsal Mark, which is effectively by Segment
        for (s = m->first(SegmentType::ChordRest); s; s = s->next( SegmentType::ChordRest)) {
            c = 0;
            for (Element* eAnn : s->annotations()) {
                switch(eAnn->type()) {
                case EType::REHEARSAL_MARK :
                case EType::TEMPO_TEXT     :
                    tick = s->tick();
                    mapSVG.insert(tick, eAnn);
                    setVTT->insert(tick);
                    c++;   // only one marker and/or one tempo per segment
                    break;
                default:
                    break;
                }
                if (c == 2)
                    break; // breaks the for loop
            }
        }
        // Bars ruler by Measure - QMultiMap is last in first out, bars last
        // here, first in file, for better cue_id sorting.
        tick = m->tick();
        mapSVG.insert(tick, static_cast<Element*>(m));
        setVTT->insert(tick);
    }

    // These templates handle height/y, the code handles width/x
    getRulersTemplate(&rectB, FILE_RULER_RB, qfi);
    getRulersTemplate(&rectM, FILE_RULER_RM, qfi);
    getRulersTemplate(&textB, FILE_RULER_TB, qfi);
    getRulersTemplate(&textM, FILE_RULER_TM, qfi);

    // Stream the <defs> and initial content from template
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_RULER_DEFS));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qtsFile.setDevice(&qf);
    *qts << qtsFile.readAll().arg(width)
                             .arg(width  - 1)
                             .arg(margin - 1)
                             .arg(endX   + 1);

    // Stream the line and text elements, with all their attributes:
    for (i = mapSVG.begin(); i != mapSVG.end(); ++i) {
        e     = i.value();
        eType = e->type();

        // Default Values:
        // x = x1 = x2, they're all the same: a vertical line or centered text.
        // The exception is x for rehearsal mark text, which is offset right.
        offX  = 0;
        label = "";
        lineID = line1;

        // Values for this cue
        tick = i.key(); // lines fall on whole number x-coordinates
        pxX = qRound(margin + (pxPerMSec * startMSecsFromTick(score, tick)));

        switch (eType) {
        case EType::MEASURE :
            iBarNo = static_cast<Measure*>(e)->no() + 1;
            if (iBarNo % 5 == 0) {
                // Multiples of 5 get a longer, thick line
                lineID = line5;
                textClass = idBars;
                if (iBarNo % 10 == 0) {
                    // Multiples of 10 get text and a shorter, thick line
                    lineID = line10;
                    label = QString("%1").arg(iBarNo);
                }
            }
            if (tick > 0) {
                x = rectX;
                rectWidth = pxX - ((pxX - lineX) / 2) - rectX;
                rectCue= prevCue;
            }
            isMarker  = false;
            isTempo   = false;
            break;
        case EType::REHEARSAL_MARK :
            offX  =  7;
            x     = pxX - offX;
            label = static_cast<const Text*>(e)->xmlText();
            lineID    = lineMrks;
            textClass = idMarkers;
            rectWidth = offX * 2;
            rectCue   = tick;
            isMarker  = true;
            isTempo   = false;
            break;
        case EType::TEMPO_TEXT :
            isMarker  = false;
            isTempo   = true;
            break;
        default:
            break;
        }

        if (isTempo)
            qtsTempo << SVG_4SPACES << SVG_GROUP_BEGIN << SVG_SPACE << SVG_SPACE << SVG_SPACE
                     << formatInt(SVG_CUE_NQ, tick, cueIdDigits, true)
                     << " data-tempo=\"" << static_cast<TempoText*>(e)->tempo()  << SVG_QUOTE
                     << SVG_GT << SVG_GROUP_END << endl;
        else {
            // Invisible <rect>s and <use>s:
            // Measuring the width of the text is dicey, thus Markers invisible
            // <rect>s cannot handle all the events, the same way Bars can. Bars
            // invisible <rect>s cover both rulers, so Markers elements must
            // stream after Bars in order to capture any events.
            // Invisible <rect>s don't need to be collected for hiLiting.
            // Bars <use> and <text> elements don't need events.
            // Too bad it has to be inconsistent across the rulers...
            // The code operates on the current Marker and the previous BarLine.
            // Marker  rect = fill horizontal space between line/text.
            // BarLine rect splits the space around each line, no empty spaces.
            if (isMarker || tick > 0) {
                pqts = (isMarker ? &qtsMarks : &qtsBars);
                elm  = (isMarker ? rectM     : rectB);
                *pqts << elm.arg(formatInt(SVG_CUE_NQ, rectCue, cueIdDigits, true))
                            .arg(formatReal(SVG_X, x, 1, xDigits, true))
                            .arg(isMarker ? "" : QString::number(rectWidth, 'f', 1));
                if (!isMarker)
                    rectX += rectWidth;
            }

            // Lines: both Markers and Bars get a line and, conditionally, text.
            pqts = (isMarker ? &qtsStyle : &qtsNoEvt);
            *pqts << SVG_4SPACES << SVG_USE << SVG_SPACE
                  << formatInt(SVG_CUE_NQ, tick, cueIdDigits, true)
                  << formatInt(SVG_X, pxX, xDigits, true)
                  << XLINK_HREF << lineID
                  << SVG_CLASS  << "OtNo" << SVG_QUOTE
                  << (isMarker ? "" : formatInt(SVG_BARNUMB, iBarNo, barNoDigits, true))
                  << SVG_ELEMENT_END << endl;

            // Text: only stream the text element if there's text inside it
            if (!label.isEmpty()) {
                elm = (isMarker ? textM : textB);
                *pqts << elm.arg(formatInt(SVG_CUE_NQ, tick, cueIdDigits, true))
                            .arg(formatInt(SVG_X, pxX + offX, xDigits, true))
                            .arg(label);
            }

            if (!isMarker) { // Bars only
                lineX   = pxX;
                prevCue = tick;
            }
        }
    } //for (i)

    // Invisible <rect> for final start-of-bar line
    rectWidth = endX - ((endX - lineX) / 2) - rectX;
    qtsBars << rectB.arg(formatInt(SVG_CUE_NQ, tick, cueIdDigits, true))
                    .arg(formatReal(SVG_X, rectX, 1, xDigits, true))
                    .arg(rectWidth);

    // Invisible <rect> for final end-of-bar line
    rectX    += rectWidth;
    rectWidth = width - rectX - 1;
    tick      = score->lastSegment()->tick();
    qtsBars << rectB.arg(formatInt(SVG_CUE_NQ, tick, cueIdDigits, true))
                    .arg(formatReal(SVG_X, rectX, 1, xDigits, true))
                    .arg(rectWidth);

    // The final end-of-bar lines
    lineID = (++iBarNo % 5 == 0 ? line5 : line1);
    qtsNoEvt << SVG_4SPACES << SVG_USE << SVG_SPACE
             << formatInt(SVG_CUE_NQ, tick, cueIdDigits, true)
             << formatInt(SVG_X, endX, xDigits, true)
             << XLINK_HREF << lineID
             << SVG_CLASS  << "OtNo" << SVG_QUOTE
             << SVG_ELEMENT_END << endl;
    qtsNoEvt << SVG_4SPACES << SVG_USE << SVG_SPACE
             << formatInt(SVG_CUE_NQ, tick, cueIdDigits, true)
             << formatInt(SVG_X, endX, xDigits, true)
             << XLINK_HREF << lineMrks
             << SVG_CLASS  << "OtNo" << SVG_QUOTE
             << SVG_ELEMENT_END << endl;

    // Stream things in order and in groups as required
    *qts << bars  << marks  << SVG_SPACE   << SVG_SPACE
         << SVG_GROUP_BEGIN << SVG_POINTER << SVG_VISIBLE << SVG_QUOTE << SVG_GT
         << endl << style   << SVG_SPACE   << SVG_SPACE   << SVG_GROUP_END
         << endl << SVG_GROUP_END            << endl
         << SVG_GROUP_BEGIN << SVG_GT        << endl
         << noEvents        << SVG_GROUP_END << endl
         << SVG_GROUP_BEGIN << SVG_GT        << endl
         << tempos          << SVG_GROUP_END << endl;

    // The loop cursors are in the footer template file
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_RULER_FTR));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qtsFile.setDevice(&qf);
    *qts << qtsFile.readAll().arg(margin).arg(endX);
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

    IntSet    setVTT;        // Start-time-only cues
    const int wRuler = 1753; // 1920 minus SMAWS_Playback.svg

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
    fileStream << qts.readAll().arg(wRuler);

    streamRulers(score, qfi, &fileStream, &setVTT, wRuler);

    // Terminate the <svg> element
    fileStream << SVG_END;

    // Write/close the SVG file
    fileStream.flush();
    rulersFile.close();

    // VTT file, start times only
    saveStartVTT(score, fnRoot, &setVTT, 0);

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
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Tables"), tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }
    else if (score->metaTag("columnCount").isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Tables"), tr("You must set the columnCount property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }
    const int nGridCols = score->metaTag("columnCount").toInt(); // number of grid columns

    // Total staves, including invible and temporarily invisible staves
    const int nStaves = score->nstaves();

    // Iterate by Staff to locate the required grid staff and optional chords staff
    const QString idChords   = "chords";
    const QString nameChords = "Chords:";
    int     idxStaff;
    int     idxGrid   = -1; // The index of the grid staff/row
    int     idxChords = -1; // The index of the optionsl chords row
    IntList twoVoices;      // For 2-voice lyrics staves
    bool    is2 = false;    // presumes only 1 voice

    for (idxStaff = 0; idxStaff < nStaves; idxStaff++) {
        if (score->staff(idxStaff)->small(0)) {
            if (score->staff(idxStaff)->partName() == STAFF_GRID)
                idxGrid = idxStaff;
            else if (score->staff(idxStaff)->partName() == idChords)
                idxChords = idxStaff;
            else if (score->staff(idxStaff)->cutaway()) { // for lack of a custom property
                twoVoices.append(idxStaff);
                is2 = true;
            }
        }
    }
    if (idxGrid < 0)
        return false; // no grid staff == no good

    int tick;
    int startOffset = 0; // this repeat's  start tick (this table's start tick)
    int pageOffset  = 0; // this page's    start tick
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
    int idxGridCol  = 0; // grid column index < columnCount
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
    IntList*        pil;        // temp variable for pitches/ordinals
    QString         cue_id;     // temp variable for cue_id
    QString         page_id;    // ditto for page cue_id
    QString         tableTitle; // RehearsalMark at startTick==0 is the title

    QStringList    tableCues; // List of cue_ids for VTT file
    StrPtrList     barNums;   // List of <text>s
    StrPtrList     barLines;  // List of <line>s
    StrPtrList     beatLines; // Every whole beat (1/4 note in 4/4 time)
    StrPtrList     gridUse;   // if (isPages) this is for grid staff
    StrPtrList     gridText;  //  ditto
    StrPtrVectVect grid(    nGridCols, 0);  // by col, by row
    StrPtrVectVect dataCues(nGridCols, 0);  // data-cue only in cells that have >0 cue_ids
    Int2IntMap     subCols;

    // Tempo
    qreal   initialBPM;     // initial tempo in BPM
    qreal   prevTempo;      // previous tempo in BPS
    int     prevTempoPage;  // previous tempo in BPS
    QString tempoCues;      // data-cue="id1;BPM1,id2:BPM2,..." for tempo changes

    const int  BPM_PRECISION = 2;
    const TempoMap* tempoMap = score->tempomap(); // a convenience

    // By page
    IntList        pageCols;     // active column count per page
    IntListList    pageBeats;    // x-coordinates by beatline, by page
    IntListList    pageBars;     // x-coordinates by barline, by page
    IntListList    pageBarNums;  // bar numbers   by barline, by page
    StrPtrListList pageGridText; // grid row innerHTML, by column, by page
    StrPtrVectList pageNames;    // instrument name text, by row, by page
    StrPtrVectList pageStyles;   // instrument name style: Lo or No, by row, by page
    StrPtrListVectVect leds(   nGridCols, 0); // data cell <use> href or <text> lyrics: by col, by row, by page
    IntListVectVect    pitches(nGridCols, 0); // MIDI note number by col, by row, by page

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
    StrPtrVectVect grid2(    nGridCols, 0);
    StrPtrVectVect dataCues2(nGridCols, 0);
    StrPtrListVectVect leds2(nGridCols, 0);

    // Constants for SVG table cell dimensions, in pixels
    const int cellWidth  =  48;
    const int cellHeight =  48;
    const int baseline   = cellHeight - 14; // text baseline for lyrics/instrument names
    const int nameLeft   =  19; // this value is duplicated in FILE_GRID_INST
    const int iNameWidth = 144; // Instrument names are in the left-most column
    const int maxDigits  =   4; // for x/y value formatting, range = 0-9999px
    const int width      = iNameWidth + (nGridCols * cellWidth);

    // Variables for SVG table cell positions
    int cellX  = 0;    // fixed offset for instrument name row headers
    int cellY  = 0;
    int height = 0;

    int colSpan, colSpan2; // negative number for tuplets and other sub divisions
    QString sme;    // 's' or 'm' or 'e' == start, middle end, for tuplets et al.
    bool isGridCol;

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
    const QString MINI = "mini";
    const QString LO   = "Lo";
    const QString NO   = "No";

    QString instNo = QString("No");
    QString instLo = QString("Lo");

    // Currently two HTML table styles, one kludgy option implementation.
    // Two styles are: tableChords and tableDrumMachine
    const QString classHTML = score->style().value(Sid::pageTwosided).toBool()
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

    const QString indy    = QString(6, SVG_SPACE); // indentation for grid row text
    const QString click   = " onclick=\"gridClick(evt)\"";
    const int cueIdDigits = 4; //!!4 = max pageOffset value: 9,999 (it's only for formatting)


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
    //   by column, then by row
    for (m = score->firstMeasure(); m; m = m->nextMeasureMM())
    {
        // startOffset = start-of-repeat or pickup bar == start of table
        // Every repeat and every pickup effectively start at tick zero
        if (m->repeatStart())
            startOffset = m->tick();

        mStartTick = m->tick() - startOffset;
        mTicks     = m->ticks();

        // For bars ruler, every start-of-bar is a cue
        tableCues.append(getCueID(mStartTick));

        for (s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest))
        {
            crGrid = s->cr(idxGrid * VOICES); // The grid staff's ChordRest
            if (crGrid != 0) {
                if (crGrid->type() == EType::CHORD)
                    isGridCol = true;
                else
                    continue; // a rest in the grid staff is ignored, bypassed
            }
            else {
                isGridCol = false;
                startTick = s->tick() - startOffset;
            }

            if (isGridCol) {                                                   /// GRID CHORD ///
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
                                isPages     = true; // this table has pages
                                isPageStart = true; // this cr is start-of-page
                                break;
                            }
                        }
                        if (isPageStart) {                                     /// NEW PAGE ///
                            cellX = 0; // reset horizontal position

                            // This page's last measure and endTick
                            mPageEnd = mp;
                            pageTick = mp->tick() + mp->ticks() - startOffset;

                            pageOffset = mStartTick;

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
                                    (*pageNames[idxPage])[eAnn->staffIdx()] =
                                        new QString(static_cast<InstrumentChange*>(eAnn)->xmlText());
                            }
                        }
                        // Reset indices for the new table or page
                        idxCol  = 0;
                        idxBeat = 0;
                        idxBar  = 0;
                    } // SVG
                } // if (new table or new page);

                spv = grid[idxCol];
                if (spv == 0) {
                    spv = new StrPtrVect(nStaves, 0);
                    grid[idxCol] = spv;
                    if (is2) {
                        spv2 = new StrPtrVect(nStaves, 0);
                        grid2[idxCol] = spv2;
                    }


                    pitches[idxCol] = new IntListVect(nStaves, 0);
                    if (isPages) {
                        dataCues[idxCol] = new StrPtrVect(nStaves, 0);
                        leds[idxCol] = new StrPtrListVect(nStaves, 0);
                        if (is2) {
                            dataCues2[idxCol] = new StrPtrVect(nStaves, 0);
                            leds2[idxCol] = new StrPtrListVect(nStaves, 0);
                        }
                    }
                }
                else if (is2)
                    spv2 = grid2[idxCol];

                gridTick  = crGrid->tick();
                gridTicks = crGrid->actualTicks();
                startTick = gridTick - startOffset;
                cellX    += (cellX == 0 ? iNameWidth : cellWidth);
            } // if (isGridCol)

            // Rows within this column
            // Iterate over all the staves and collect stuff
            for (int r = 0; r < nStaves; r++) {
                const int track = r * VOICES;
                const bool isChordsRow = (r == idxChords);

                // The ChordRest for this staff, using only Voice #1
                crData  = s->cr(track);
                isChord = (crData != 0);

                if (!isGridCol && !isChord)
                    continue; // tuplet or other sub-div, but not in this staff

                if (r == idxGrid) {                                            /// GRID CELL ///
                    // Grid staff cells are simple
                    pqs = (*spv)[r];
                    if (!isPages || pqs == 0) {
                        pqs = new QString;
                        (*spv)[r] = pqs;
                    }
                    qts.setString(pqs);

                    if (isHTML)
                        qts << HTML_TH_BEGIN << SVG_GT
                            << crGrid->lyrics()[0]->plainText()
                            << HTML_TH_END;
                    else {
                        // Required if the idxGrid is not the first staff (which may not be plausible anyway...)
                        cue_id = getCueID(startTick, startTick + gridTicks);
                        tableCues.append(cue_id);

                        QString ref = STAFF_GRID;
                        ref += (isPages && idxPage > 0 ? LO: NO);
                        tick = startTick - pageOffset;
                        // Grid <use> element
                        if (!isPages || idxCol == gridUse.size()) {
                            qts << SVG_USE  << SVG_SPACE
                                << formatInt(SVG_X, cellX, maxDigits, true)
                                << formatInt(SVG_Y, cellY, 2, true)
                                << XLINK_HREF  << ref  << SVG_QUOTE
                                << formatInt(SVG_COL_CUE, tick, cueIdDigits, true)
                                << SVG_CUE << cue_id;

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

                        // grid <text> LO == invisible == <text></text>
                        if (!isPages || idxCol == gridText.size()) {
                            qts << SVG_TEXT_BEGIN
                                << formatInt(SVG_X, cellX + (cellWidth  / 2), maxDigits, true)
                                << formatInt(SVG_Y, cellY + (cellHeight / 2), 2, true)
                                << indy << SVG_CLASS << CLASS_GRID << NO << SVG_QUOTE
                                << formatInt(SVG_COL_CUE, tick, cueIdDigits, true)
                                << SVG_CUE << cue_id;

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
                        const int denom = mTicks
                                        / score->staff(r)->timeSig(startTick)->sig().denominator();

                        tick  = startTick - mStartTick;
                        if (tick && !(tick % denom)) {
                            if (!isPages || idxBeat == beatLines.size()) {
                                // Initial x-coord for line: if it's not
                                // page 1, it's invisible via negative x.
                                pqs = new QString;
                                qts.setString(pqs);
                                qts << SVG_LINE               // + 0.5 because they're 1px vertical lines
                                       << formatInt(SVG_X1_NQ, x + 0.5, maxDigits + 1, true)
                                       << formatInt(SVG_Y1_NQ, cellHeight
                                                             + gridMargin, 2, true)
                                       << formatInt(SVG_X2_NQ, x + 0.5, maxDigits + 1, true)
                                       << formatInt(SVG_Y2_NQ, height
                                                             - cellHeight
                                                             - gridMargin, maxDigits, true)
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
                else {                                                         /// DATA CELL (Notes/Rests) ///
                    if (!isPages || isPageStart)
                        isStaffVisible[r] = m->system()->staff(r)->show(); // coarse setting

                    // Set instrument name for this row once per table
                    //                 and if (isPages) once per page
                    if (isPageStart || (iNames[r] == 0 && isStaffVisible[r]))  /// INSTRUMENT NAMES ///
                    {
                        if (isPageStart && isStaffVisible[r]) {
                            // Empty staff within a page is "invisible"
                            for (mp = m; mp; mp = mp->nextMeasureMM()) {
                                if (!mp->isOnlyRests(track))
                                    break;
                                if (mp == mPageEnd) {                      // empty staff for this page
                                    isStaffVisible[r] = false;             // fine setting
                                    break;
                                }
                            }
                        }
                        if (iNames[r] == 0 || isPageStart) {
                            if (iNames[r] == 0) {
                                iNames[r] = new QString;
                                if (!isChordsRow) { //!!no initial class value for chords row hdr!!
                                    qts.setString(iNames[r]);
                                    qts << SVG_CLASS << "iName" << SVG_QUOTE;
                                }
                            }
                            if (isPageStart)
                                (*pageStyles[idxPage])[r] = (isStaffVisible[r]
                                                             ? &instNo
                                                             : &instLo);
                        }
                    }

                    if (isChord) {
                        isChord &= crData->isChord() && crData->visible();
                        if (isChord)
                            note = static_cast<Chord*>(crData)->notes()[0];
                    }

                    // Small staves are Grid, Chords, or Lyrics (all have text)
                    isLED = !score->staff(r)->small(0);

                    // Lyrics can have two voices in one staff, rest == empty
                    const bool has2 = twoVoices.indexOf(r) >= 0; // does this staff have 2 voices?
                    if (has2) {
                        spv2 = grid2[idxCol];
                        if (spv2 == 0) {
                            spv2 = new StrPtrVect(nStaves, 0);
                            grid2[idxCol] = spv;
                        }

                        cr2 = s->cr(track + 1);
                        isChord2 = (cr2 != 0 && cr2->isChord() && cr2->visible());
                        if (isChord2) {
                            note2 = static_cast<Chord*>(cr2)->notes()[0];
                            isChord2 = (note2->tieBack() == 0); // exclude secondary tied notes
                        }
                    }
                    else
                        isChord2 = false;

                    // isChord and isChord2 have different rules:
                    //  - isChord2 is lyrics only, rest == empty
                    //  - isChord == false == not empty == rest in voice 1 LED
                    if (isGridCol
                     && (!isStaffVisible[r]
                      || (!isChord2 && (crData == 0
                                     || (isChord && note->tieBack() != 0)
                                     || (!isLED  && !isChord)))))
                    {                                                          /// EMPTY CELL ///
                        if (!isHTML && (isPages || isStaffVisible[r])) {
                            cellY += cellHeight; // for the next row
                        }
                        if (isPages) {
                            if ((*pitches[idxCol])[r] != 0)
                                (*pitches[idxCol])[r]->append(MIDI_EMPTY);
                            if ((*leds[idxCol])[r] != 0) {
                                pqs = (isLED ? new QString(SVG_HASH) : new QString);
                                (*leds[idxCol])[r]->append(pqs);
                            }
                            if (has2 && (*leds2[idxCol])[r] != 0) {
                                pqs = new QString;
                                (*leds2[idxCol])[r]->append(pqs);
                            }
                        }
                        continue; // empty cell complete, on to the next staff
                    }

                    if (isChord)
                        dataTicks = note->playTicks(); // handles tied notes, secondary tied notes excluded above
                    else if (!has2)
                        dataTicks = crData->actualTicks();
                    else
                        dataTicks = gridTicks; // makes things simpler in code below

                    if (dataTicks >= gridTicks) {
                        colSpan  = dataTicks / gridTicks;
                        sme.clear();
                    }
                    else {
                        colSpan  = gridTicks / dataTicks * -1;
                        if (isGridCol)
                            sme = "s";
                        else if (startTick + dataTicks == gridTick + gridTicks)
                            sme = "e";
                        else
                            sme = "m";
                    }

                    if (isChord2) {
                        dataTicks2 = (isChord2 ? note2->playTicks() : gridTicks);

                        colSpan2 = (dataTicks2 >= gridTicks
                                    ? dataTicks2 / gridTicks
                                    : gridTicks  / dataTicks2 * -1);
                    }

                    if (!isGridCol) {                                          /// TUPLET or other non-grid sub-division ///
                        // Is there already a column for this?
                        if (subCols.find(startTick) == subCols.end()) {
                            pitches.append(new IntListVect(nStaves, 0));
                            if (isPages) {
                                dataCues.append(new StrPtrVect(nStaves, 0));
                                leds.append(new StrPtrListVect(nStaves, 0));
                                if (is2) {
                                    dataCues2.append(new StrPtrVect(nStaves, 0));
                                    leds2.append(new StrPtrListVect(nStaves, 0));
                                }
                            }
                            spv = new StrPtrVect(nStaves, 0);
                            grid.append(spv);
                            if (is2) {
                                spv2 = new StrPtrVect(nStaves, 0);
                                grid2.append(spv2);
                            }
                            subCols[startTick] = grid.size() - 1;
                        }
                        idxGridCol = idxCol; // to restore idxCol later
                        idxCol     = subCols[startTick];
                    }

                    cue_id = getCueID(startTick, startTick + dataTicks);
                    if (isChord2)
                        cue_id2 = getCueID(startTick, startTick + dataTicks2);

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

                    if (isHTML) {
                        // Start the <td> element
                        qts << HTML_TD_BEGIN;
                        if (colSpan > 1)
                            // Usually cells only span one column but not always.
                            // Add cue_id and colspan attributes to this <td>.
                            qts << SVG_CUE   << cue_id
                                << SVG_QUOTE << HTML_COLSPAN << colSpan << SVG_QUOTE;
                        qts << SVG_GT;

                        // <td>content</td> only if it's a note in this staff
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
                            cellValue = QString("%1%2%3%4")
                                       .arg(LED)
                                       .arg(colSpan != 1 ? QString::number(colSpan) : "")
                                       .arg(sme)
                                       .arg(isChord ? NO : LO);
                        else if (isChordsRow) {
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

                        tick = startTick - pageOffset;
                        if (pqs->isEmpty()) {
                            if (isLED) { // LED staff, notes not text
                                qts << SVG_USE << SVG_SPACE
                                    << formatInt(SVG_X, cellX, maxDigits, true)
                                    << SVG_Y       << SVG_PERCENT // for multi-pitch
                                    << formatInt(SVG_COL_CUE, tick, cueIdDigits, true)
                                    << XLINK_HREF;

                                if (!isPages || idxPage == 0)
                                    qts << cellValue;

                                qts << SVG_QUOTE;
                            }
                            else if (isChord) {
                                y = baseline - (has2 ? 15 : 1);
                                qts << SVG_TEXT_BEGIN
                                    << formatInt(SVG_X, x, maxDigits, true)
                                    << formatInt(SVG_Y, y, 2, true)
                                    << SVG_CLASS << (isChordsRow ? chord : lyric)
                                    << NO << SVG_QUOTE
                                    << formatInt(SVG_COL_CUE, tick, cueIdDigits, true);
                            }
                        }
                        if (isChord2 && pqs2->isEmpty()) {
                            y = baseline + 5;
                            qts2 << SVG_TEXT_BEGIN
                                 << formatInt(SVG_X, x, maxDigits, true)
                                 << formatInt(SVG_Y, y, 2, true)
                                 << SVG_CLASS << lyric << NO << SVG_QUOTE
                                 << formatInt(SVG_COL_CUE, tick, cueIdDigits, true);
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
                                qts << (pqs->isEmpty() ? SVG_CUE : QString(SVG_COMMA))
                                    << cue_id;
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
                                qts2 << (pqs2->isEmpty() ? SVG_CUE : QString(SVG_COMMA))
                                     << cue_id2;

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
                        } // if (isPages)

                        if (colSpan != 1 || !isGridCol) // additional, non-grid cue_id
                            tableCues.append(cue_id);

                        if (isChord2 && (colSpan2 != 1 || !isGridCol))
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

            if (!isGridCol)
                idxCol = idxGridCol; // restore this to normal grid alignment
            else if (isPages)
                idxCol++;

            height = cellY + cellHeight; // Extra row for title/buttons
            cellY  = 0;
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
                                                << stringToUTF8(score->staff(r)->part()->shortName(startOffset), true);
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

                // Stream the SVG header elements <svg>, <desc>.
                // No <title> because it displays as tooltip for the entire doc.
                tableStream
                    << SVG_BEGIN
                       << XML_NAMESPACE << endl          << XML_XLINK
        << SVG_4SPACES << SVG_VIEW_BOX  << SVG_ZERO      << SVG_SPACE
                                        << SVG_ZERO      << SVG_SPACE
                                        << width         << SVG_SPACE
                                        << height        << SVG_QUOTE
                       << SVG_HEIGHT    << height        << SVG_QUOTE << endl
        << SVG_4SPACES << SVG_POINTER   << SVG_NONE      << SVG_QUOTE << endl
        << SVG_4SPACES << (hasRulers    ?  SVG_ONLOAD : "")
                       << (isPages      ?  pageCues   : "") << SVG_GT << endl
                    << SVG_DESC_BEGIN   << smawsDesc(score) << SVG_DESC_END << endl;

                // Import the <defs>
                QFile qf;
                qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_GRID_DEFS));
                qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
                qts.setDevice(&qf);
                tableStream << qts.readAll() << endl;

                // The background rects (pattern + gradient in one fill == not)
                qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_GRID_BG));
                qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
                qts.setDevice(&qf);
                tableStream << qts.readAll()
                                  .replace("%1", QString::number(width))
                                  .replace("%2", QString::number(height))
                            << endl;

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
                tableStream << SVG_GROUP_BEGIN << SVG_GT << endl;
                for (int b = 0; b < barLines.size(); b++)
                    tableStream << SVG_2SPACES << *barLines[b];
                tableStream << SVG_GROUP_END   << endl;

                tableStream << SVG_GROUP_BEGIN << SVG_GT << endl;
                for (int b = 0; b < barNums.size(); b++)
                    tableStream << SVG_2SPACES << *barNums[b];
                tableStream << SVG_GROUP_END   << endl;

                // Stream the beatLines (beat and barline/barnum loops could be consolidated in a function, maybe?)
                tableStream << SVG_GROUP_BEGIN << SVG_GT << endl;

                for (int b = 0; b < beatLines.size(); b++) {
                    tableStream << SVG_2SPACES << *beatLines[b];

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
                tableStream << SVG_GROUP_END << endl;

                if (isPages) {
                    // Stream the grid staff separately
                    int minCols = nGridCols;       // min # of columns per page
                    for (int p = 0; p < idxPage; p++) {
                        if (pageCols[p] < minCols)
                            minCols = pageCols[p];
                    }

                    tableStream << SVG_GROUP_BEGIN << click
                                << SVG_POINTER     << SVG_VISIBLE << SVG_QUOTE
                                << SVG_GT          << endl;

                    // Stream the <use> and <text> into separate <g>s for
                    // improved javascript load performance
                    tableStream << SVG_2SPACES << SVG_GROUP_BEGIN << SVG_GT << endl;
                    for (int g = 0; g < nGridCols; g++) {
                        tableStream << SVG_4SPACES << *gridUse[g];
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
                        tableStream << SVG_QUOTE << SVG_ELEMENT_END << endl;
                    }

                    tableStream << SVG_2SPACES << SVG_GROUP_END   << endl
                                << SVG_2SPACES << SVG_GROUP_BEGIN << SVG_GT << endl;

                    for (int g = 0; g < nGridCols; g++) {
                        tableStream << SVG_4SPACES << *gridText[g];

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
                    tableStream << SVG_2SPACES   << SVG_GROUP_END << endl
                                                 << SVG_GROUP_END << endl;
                }

                const int colCount = grid.size(); // max # of columns per page

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
                QString id, name, title, ctrls;
                qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_GRID_INST));
                qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
                qts.setDevice(&qf);
                ctrls = qts.readAll();
                cellY = 0;

                tableStream << SVG_GROUP_BEGIN << SVG_GT << endl; // easier to navigate in javascript
                for (int r = 0; r < nStaves; r++) {
                    const bool isGridRow   = (r == idxGrid);
                    const bool isChordsRow = (r == idxChords);
                    QStringList names;

                    // Stream the instrument names - similar loop to bar/beatlines
                    if (!isGridRow && iNames[r] != 0) {
                        if (isChordsRow) {
                            id   = idChords;
                            name = nameChords;
                        }
                        else {
                            // Display name (text element's innerHTML)
                            if ((*pageNames[0])[r] == 0 || (*pageNames[0])[r]->isEmpty()) {
                                QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Tables"), tr("There is a staff without an initial instrument name!"));
                                return false;
                            }
                            names = (*pageNames[0])[r]->split(SVG_PERCENT);
                            if (names.size() < 3) {
                                QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Tables"), tr("There is a staff without all 3 parts of the initial instrument name!"));
                                return false;
                            }
                            name  = names[0];
                            id    = stringToUTF8(stripNonCSS(name)); // id value cannot contain spaces
                            name  = stringToUTF8(name, true);
                        }
                        isLED = !score->staff(r)->small(0);

                        // Each staff (row) is wrapped in a group
                        tableStream << SVG_SPACE << SVG_GROUP_BEGIN
                                    << SVG_ID    << id << SVG_QUOTE;

                        if (!isLED && !isChordsRow)
                            tableStream << SVG_CLASS << "lyrics" << SVG_QUOTE;

                        tableStream << SVG_TRANSFORM << SVG_TRANSLATE << SVG_ZERO
                                    << SVG_SPACE     << cellY << SVG_RPAREN_QUOTE
                                    << SVG_GT << endl;

                        if (!isChordsRow) { // <title> = tooltip long name
                            title = (names.size() > 1 && !names[1].isEmpty()
                                     ? stringToUTF8(names[1], true)
                                     : name);

                            tableStream << ctrls.arg(title)
                                        << SVG_FILL_URL << names[2] << SVG_RPAREN_QUOTE
                                        << *iNames[r];
                        }
                        else                  // the one and only chords row
                            tableStream << SVG_2SPACES << SVG_TEXT_BEGIN
                                        << formatInt(SVG_X, nameLeft, 2, true)
                                        << formatInt(SVG_Y, baseline, 2, true);

                        if (isPages) {
                            bool changesName  = false;
                            if (r != idxChords) { // chords staff never changes name
                                pqs = (*pageNames[0])[r];
                                for (int p = 1; p < idxPage; p++) {
                                     if ((*pageNames[p])[r] != 0) { // if user creates identical intrument changes, it's their own fault.
                                        changesName = true;
                                        break;
                                    }
                                }
                            }
                            bool changesStyle = false;
                            pqs = (*pageStyles[0])[r];
                            for (int p = 1; p < idxPage; p++) {
                                if ((*pageStyles[p])[r] != pqs) {
                                    changesStyle = true;
                                    break;
                                }
                            }
                            if (changesName || changesStyle) { // page cues required
                                bool isName, isStyle;
                                QString* pqN;
                                QString* pqS;
                                tableStream << SVG_CUE;
                                for (int p = 0; p < idxPage; p++) {
                                    pqN = (*pageNames[p])[r];
                                    pqS = (*pageStyles[p])[r];

                                    if (p == 0)
                                        isStyle = true;
                                    else {
                                        tableStream << SVG_COMMA;
                                        isStyle = (changesStyle && pqS != (*pageStyles[p - 1])[r]);
                                    }
                                    tableStream << pageIDs[p];

                                    isName = (changesName && pqN != 0);
                                    if (isName || isStyle) {
                                            tableStream << SVG_SEMICOLON;
                                        if (isName)
                                            tableStream << *pqN;
                                        if (isStyle)
                                            tableStream << SVG_SEMICOLON << *pqS;
                                    }
                                }
                                tableStream << SVG_QUOTE;
                            }
                        }
                        // Stream the >content</text>
                        tableStream << SVG_GT << name << SVG_TEXT_END << endl;

                        // Spacer below staff = instLine below staff in SVG
                        // Spacer must be a down spacer in the first measure
                        if (m1->vspacerDown(r) != 0) {
                            const int y = cellHeight; // * (r + 1);
                            const int xMargin = 3;
                            if (isPages)
                                tableStream << SVG_4SPACES; // indentation inside group

                            tableStream << SVG_LINE
                                        << SVG_X1    << xMargin         << SVG_QUOTE
                                        << SVG_Y1    << y               << SVG_QUOTE
                                        << SVG_X2    << width - xMargin << SVG_QUOTE
                                        << SVG_Y2    << y               << SVG_QUOTE
                                        << SVG_CLASS << "instLine"      << SVG_QUOTE
                                        << SVG_ELEMENT_END << endl;
                        }
                    }

                    // The data cells (and grid cells if isPage == false)
                    if (isPages && !isGridRow) {
                        tableStream << SVG_2SPACES << SVG_GROUP_BEGIN
                                    << SVG_CLASS   << CLASS_NOTES << SMAWS_GRID
                                    << SVG_QUOTE   << click;

                        if (!isChordsRow)
                            tableStream << SVG_FILL_URL << names[2] << SVG_RPAREN_QUOTE;

                        tableStream << SVG_GT << endl;
                    }

                    const bool hasPitches = (pitchSet[r] != 0
                                          && pitchSet[r]->size() > 1);

                    if (!(isPages && isGridRow)) {
                        for (int c = 0; c < grid.size(); c++) {
                            if ((*grid[c])[r] == 0)
                                continue;

                            int  pitch0;
                            qreal y = 0;

                            pqs = (*grid[c])[r];
                            if (isLED) {
                                if (hasPitches) {
                                    pil    = (*pitches[c])[r];
                                    pitch0 = (*pil)[0];
                                    const int idx = pitchSet[r]->indexOf(pitch0);
                                    y = (idx >= 0 ? intervals[r] * idx : restOffset);
                                    pqs->replace(LED, MINI);
                                }
                                pqs->replace(QString("%1%2")    .arg(SVG_Y).arg(SVG_PERCENT),
                                             QString("%1%2%3%4").arg(SVG_Y).arg(SVG_QUOTE)
                                                                .arg(y)    .arg(SVG_QUOTE));
                            }

                            if (isPages)
                                tableStream << SVG_4SPACES; // indentation inside group

                            tableStream << *pqs;

                            if (isPages) {                        // ¿Page Cues?
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
                                        tableStream << (p == 0 && !hasCues ? SVG_CUE : QString(SVG_COMMA))
                                                    << pageIDs[p] << SVG_SEMICOLON;

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
                            if (isPages)
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
                    } // if (!(isPages && r == idxGrid))

                    if (isPages && r != idxGrid)
                        tableStream << SVG_2SPACES << SVG_GROUP_END << endl
                                    << SVG_SPACE   << SVG_GROUP_END << endl;
                    cellY += cellHeight;
                } // for each row
                tableStream << SVG_GROUP_END << endl;

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
                    tableStream << SVG_GROUP_BEGIN  << SVG_ID        << "markers"
                                << SVG_QUOTE        << SVG_TRANSFORM << SVG_TRANSLATE
                                << SVG_ZERO         << SVG_SPACE     << height
                                << SVG_RPAREN_QUOTE << SVG_GT        << endl;

                    IntSet setVTT;
                    streamRulers(score, qfi, &tableStream, &setVTT, width);

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
                tableStream << qts.readAll().replace("%0", tempoCues)
                                            .replace("%1", QString::number(initialBPM, 'f', BPM_PRECISION));
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
//!!only svg, only 1 table per score (for now)!!            width     = 0;
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
            const int n  = m->no() + 2; // this is the bar number for the next bar
            const int cX = cellX + cellWidth; // the lines are to the right of the leds
            const int x  = (isPages && idxPage > 0 ? -100 : cX - barRound);
            if (!isHTML
            && (!isPages || (idxBar == barLines.size() && m != mPageEnd))) {
                // End-of-Bar lines for the all but the last bar of the pattern.
                // <rect> because <line>s are funky with gradients.
                pqs = new QString;
                qts.setString(pqs);
                qts << SVG_RECT
                       << formatInt(SVG_X, x,             maxDigits + 1, true)
                       << formatInt(SVG_Y, barMargin - 2, 2, true)
                       << SVG_WIDTH  << barWidth  << SVG_QUOTE
                       << SVG_HEIGHT << height - cellHeight - barMargin - 2
                                                  << SVG_QUOTE
                       << SVG_RX     << barRound  << SVG_QUOTE
                       << SVG_RY     << barRound  << SVG_QUOTE
                       << SVG_CLASS  << "barLine" << SVG_QUOTE;
                if (!isPages)
                    qts << SVG_ELEMENT_END   << endl;

                barLines.append(pqs);

                // Bar numbers for the bar lines
                pqs = new QString;
                qts.setString(pqs);
                qts << SVG_TEXT_BEGIN
                       << formatInt(SVG_X, x + 4,     maxDigits + 1, true)
                       << formatInt(SVG_Y, barMargin, 2, true)
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
                pageBars[idxBar]->append(cX - barRound);
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
    Chord* chord = static_cast<Chord*>(cr);

    foreach(Articulation* a, chord->articulations()) {
          if (a->symId() == SymId::stringsDownBow)
              return PICK_UP;
    }
    return PICK_DOWN;
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

    Staff*          staff;
    Staff*          ls;   // linked staff
    Measure*        m;
    Segment*        s;
    ChordRest*      cr;
    LinkedElements* le;

    QString*    pqs;
    QTextStream qts;
    int         i, j;
    int         str;
    int         nStrings;
    int         tick;
    int         lastTick;
    int         pitch;
    int         x;
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
    StrPtrVectList tuningText;
    const int   SVG_FRET_NO = -999; // noLite y-coord for fingers. No headstock will be that tall.
    const int   MARGIN      = 20;   // margin between staves and for fret numbers
    const qreal RULE_OF_18  = 17.817154;

    // What staves are we using? Does the tab staff pair with a notation staff?
    x = 0;
    for (i = 0; i < score->nstaves(); i++) {
        staff = score->staves()[i];
        if (staff->invisible())
            continue;

        if (staff->isTabStaff(0)) {
            nStrings = staff->staffType(0)->lines();
            spv = new StrPtrVect(nStrings);
            for (str = 0; str < nStrings; str++)
                (*spv)[str] = new QString();
            values.append(spv);

            pil = new IntList();
            for (str = 0; str < nStrings; str++)
                pil->append(0);
            lastTicks.append(pil);

            stavesTab.append(i);

            le = staff->links();
            if (le) {
                for(j = 0; j < le->size(); ++j) {
                    ls = static_cast<Staff*>(le->at(j));
                    if (!ls->isTabStaff(0)) {
                        stavesTPC.append(ls->idx());
                        break;
                    }
                }
            }
            else if (i > 0 // legacy old skool, tabs + notes w/o linked staves
                  && score->staves()[i - 1]->part()->longName() == staff->part()->shortName())
                stavesTPC.append(i - 1);
            else
                stavesTPC.append(i);

            // Open string (noLite) MIDI pitch number and unicode text values
            spv = new StrPtrVect(nStrings);
            for (str = 0; str < nStrings; str++) {
                // The list itself is in opposite order from note.string() return value. Messy, but true
                pitch = staff->part()->instrument()->stringData()->stringList()[nStrings - str - 1].pitch;
                (*spv)[str] = new QString();
                qts.setString((*spv)[str]);
                qts << SVG_SPACE << pitch << SVG_SPACE
                    << tpc2unicode(pitch2tpc(pitch,
                                             score->staves()[stavesTPC[x]]->key(0),
                                             Prefer::NEAREST),
                                   NoteSpellingType::STANDARD,
                                   NoteCaseType::UPPER)
                    << SVG_SPACE;
            }
            tuningText.append(spv);

            // x offset for this fretboard, accumulated across staves as width
            xOffsets.append((nStrings * 21) + MARGIN + 6); // fret numbers + 6=borders
            width += xOffsets[x];
            x++;
        }
    }

    // Iterate chronologically left-to-right, staves top-to-bottom
    //   by measure, by segment of type ChordRest
    for (m = score->firstMeasure(); m; m = m->nextMeasureMM())
    {
        for (s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest))
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
                        // e.g. 123456 E 5 dnPick,
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
                                << lastTick    << *(*tuningText[i])[str]
                                << SVG_FRET_NO << SVG_SPACE << PICK_NO;
                        }

                        (*pil)[str] = tick + cr->actualTicks();

                        setVTT.insert(tick);
                        qts << (pqs->isEmpty() ? SVG_CUE : QString(SVG_COMMA))
                            << tick                          << SVG_SPACE
                            << note->pitch()                 << SVG_SPACE
                            << spellUnicode(s, stavesTab[i], stavesTPC[i], n, note)
                                                             << SVG_SPACE
                            << note->fret()                  << SVG_SPACE
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
                    << tick        << *(*tuningText[i])[str]
                    << SVG_FRET_NO << SVG_SPACE << PICK_NO;
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
    file << SVG_BEGIN   << XML_NAMESPACE << XML_XLINK
         << SVG_4SPACES << SVG_VIEW_BOX  << SVG_ZERO << SVG_SPACE
                                         << SVG_ZERO << SVG_SPACE
                                         << width    << SVG_SPACE
                                         << height   << SVG_QUOTE
                        << SVG_WIDTH     << width    << SVG_QUOTE << endl
         << SVG_4SPACES << SVG_XYMIN_MEET                         << endl
         << SVG_4SPACES << SVG_POINTER   << SVG_NONE << SVG_QUOTE
                        << SVG_CURSOR    << SVG_GT                << endl;

    // The <defs>
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_FRET_DEFS));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);
    file << qts.readAll();

    // The <g id="Staves"> wrapper for the staves
    file << SVG_GROUP_BEGIN << SVG_ID << ID_STAVES << SVG_QUOTE << SVG_GT << endl;

    // The staves as fretboards
    x = 0;
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

        x += (i > 0 ? xOffsets[i - 1]: 0);
        qs = qts.readAll();
        qs.replace("%id", stringToUTF8(stripNonCSS(
                   score->staves()[stavesTab[i]]->part()->longName())));  // staff id attribute, same as tab staff in score
        qs.replace("%x", QString::number(x));  // The tranlate(x 0) coordinate for this staff - fretboards are initially vertical, stacked horizontally
        for (str = 0; str < nStrings; str++)
            qs.replace(QString("\%%1").arg(str), *(*values[i])[str]);

        file << qs;
    }

    // The buttons (best last, as they must always be on top)
    qf.setFileName(QString("%1/%2").arg(qfi->path()).arg(FILE_FRET_BUTTS));
    qf.open(QIODevice::ReadOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);
    file << qts.readAll().arg(width - MARGIN);

    file << SVG_GROUP_END << endl;           // Terminate the Staves group
    file << SVG_END;                         // Terminate the <svg>
    file.flush();                            // Write the file
    qfSVG.close();                           // Close the SVG file
    qf.close();                              // Close the read-only file
    saveStartVTT(score, fnRoot, &setVTT, 0); // Write the VTT file
    return true;                             // Return success
}

//
// saveSMAWS_Tree()
// Special characters prefixed to node names to differentiate cue types
//   ,  separates nodes within a cue
//   !  ruler gray-out cue
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
        const bool isPulse = score->staff(idx)->small(0);

        // 1 Intrument Name = comma-separated list of MixTree node names
        QString iName = stringToUTF8(score->staves()[idx]->part()->longName());

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
                if (!isPulse && !isPrevRest) {
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
                for (Segment* s = m->first(SegmentType::ChordRest);
                              s;
                              s = s->next(SegmentType::ChordRest))
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
                            s = cr->segment(); // ¡Updates the pointer used in the inner for loop!
                        }
                        break;
                    case EType::REST :
                        if (!isPulse && !isPrevRest) {
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
// saveSMAWS_Lyrics() - produces one VTT file and one HTML text file.
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
    bool    isLyric;
    bool    isChord;
    bool    isOmit = false;
    bool    isPrevRest;
    bool    isPrevOmit;
    bool    isItalic;
    QString lyricsStaff;
    QString lyricsEmpty;
    QString lyricsVTT;
    QString lyricsHTML;
    IntSet  setVTT;                      // std::set of mapHTML.uniqueKeys()
    QMultiMap<QString, QString> mapVTT;  // subtitles:   key = cue_id,    value = lyrics text
    QMultiMap<int,     QString> mapHTML; // lyrics html: key = startTick, value = lyrics text

    RehearsalMark* rm;

    const QString br           = "<br>";
    const QString beginItalics = "<i>";
    const QString endItalics   = "</i>";
    Articulation* art = new Articulation(score);
    art->setSymId(SymId::stringsDownBow);

    // Iterate over the lyrics staves and collect cue_ids + lyrics text
    // By staff by measure by segment of type ChordRest
    for (i = 0; i < score->nstaves(); i++)
    {
        isPrevRest = true;
        isLyric    = score->staff(i)->hideSystemBarLine(); // it's a reasonable property to hijack for this purpose

        // VTT+CSS-class version of part name, enclosed in angle brackets.
        // VTT cue text looks like this:
        //     <c.partName>insert-lyrics-here</c>
        // CSS rule looks like this:
        //     .partName {...}
        // VTT speaker name is a <v name></v> block (not implemented yet, use staff.instrument.shortName)
        lyricsStaff = QString("%1%2").arg(VTT_CLASS_BEGIN).arg(score->staff(i)->partName());

        for (Measure* m = score->firstMeasure(); m; m = m->nextMeasureMM()) {
            for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest))
            {
                ChordRest* cr = s->cr(i * VOICES);
                if (cr == 0)
                    continue;

                tick   = cr->tick();
                isChord = cr->isChord();
                if (isChord) {
                    // Chords with downbows are treated like rests
                    Chord* ch = static_cast<Chord*>(cr);
                    isOmit = ch->hasArticulation(art);
                    if (cr->lyrics().size() > 0) {
                        if (isPrevRest) { // New line of lyrics
                            startTick = tick;
                            lyricsVTT = lyricsStaff;
                            isItalic  = cr->lyrics()[0]->textBlock(0).formatAt(0)->italic();

                            if (isItalic) {
                                lyricsVTT += "Italic";
                                lyricsHTML = beginItalics;
                            }
                            else
                                lyricsHTML.clear();

                            lyricsVTT += SVG_SPACE;
                        }
                        else {            // Add to existing line of lyrics
                            switch (cr->lyrics()[0]->syllabic()) {
                            case Lyrics::Syllabic::SINGLE :
                            case Lyrics::Syllabic::BEGIN  :
                                lyricsVTT += SVG_SPACE;   // new word, precede it with a space
                                if (isLyric && !isOmit)
                                    lyricsHTML += SVG_SPACE;
                                break;
                            default :
                                break; // multi-syllable word with separate timestap tags, no preceding space
                            }
                        }

                        lyricsVTT += SVG_LT;
                        lyricsVTT += ticks2VTTmsecs(tick, tempos);
                        lyricsVTT += SVG_GT;
                        lyricsVTT += cr->lyrics()[0]->plainText(); ///!!!Looping over lyricsList vector will be necessary soon, tied in with repeats, which are also 100% unhandled in SMAWS today!!! though for animated scores it may not be necessary.  Repeats require multiple cue_ids per animated note.

                        if (isLyric && !isOmit)
                            lyricsHTML += cr->lyrics()[0]->plainText();
                    }
                    isPrevOmit = isOmit;
                }
                else { // It's a rest
                    if (!isPrevRest && tick != 0) {
                        lyricsVTT += VTT_CLASS_END;
                        mapVTT.insert(getCueID(startTick, tick), lyricsVTT);

                        if (isLyric && !isPrevOmit) {
                            if (isItalic)
                                lyricsHTML += endItalics;
                            mapHTML.insert(startTick, lyricsHTML);
                            setVTT.insert(startTick);
                        }
                    }

                    // RehearsalMarks on rests are used to create non-lyrics
                    // lines in the HTML file. These are necessary to animate
                    // spaces between verses properly, intro/outro/solo sections too.
                    if (!isChord && isLyric) { // no good way to restrict this further, every rest in this staff...
                        for (Element* eAnn : s->annotations()) {
                            if (eAnn->type()  == EType::REHEARSAL_MARK
                             && setVTT.find(tick) == setVTT.end())
                            { // rehearsal marks are system-level, not staff-level
                                rm       = static_cast<RehearsalMark*>(eAnn);
                                isItalic = rm->textBlock(0).formatAt(0)->italic();

                                if (isItalic)
                                    lyricsEmpty = beginItalics;
                                else
                                    lyricsEmpty.clear();
                                lyricsEmpty += rm->xmlText();
                                if (isItalic)
                                    lyricsEmpty += endItalics;
                                if (lyricsEmpty.trimmed().isEmpty())
                                    lyricsEmpty = br;\

                                mapHTML.insert(tick, lyricsEmpty);
                                setVTT.insert(tick);
                                break; // only one marker per segment
                            }
                        }
                    }
                }
                isPrevRest = !isChord; // for the next segment

            } // for each segment
        } // for each measure
    } // for each staff

    // If the chord lasts until the end of the score
    if (!isPrevRest) {
        lyricsVTT += VTT_CLASS_END;
        mapVTT.insert(getCueID(startTick, score->lastSegment()->tick()), lyricsVTT);
        if (!lyricsHTML.isEmpty())
            if (isItalic)
                lyricsHTML += endItalics;
            mapHTML.insert(startTick, lyricsHTML);
            setVTT.insert(startTick);
    }

    // It's file time
    QFile       qf;
    QTextStream qts;
    QStringList keys;
    QStringList values;

    const QString fnRoot   = QString("%1/%2").arg(qfi->path()).arg(score->metaTag(tagWorkNo));
    const QString fnLyrics = QString("%1%2%3").arg(fnRoot).arg(SMAWS_).arg(SMAWS_LYRICS);

    // Subtitles VTT file:
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

    // HTML file:
    // Collect the cues into a string, iterating by cue_id, and calculating y
    lyricsHTML.clear();
    qts.setString(&lyricsHTML);
    for (IntSet::iterator tick = setVTT.begin(); tick != setVTT.end(); ++tick) {
        values = mapHTML.values(*tick);
        for (QStringList::iterator i = values.begin(); i != values.end(); ++i)
            qts << "<span"
                   << formatInt(SVG_CUE_NQ, *tick, CUE_ID_FIELD_WIDTH, true)
                   << SVG_CLASS << "lyricsOtNo" << SVG_QUOTE
                   << SVG_GT    << *i
                << "</span>" << endl;
    }

    // Open a stream into the HTML file
    qf.setFileName(QString("%1%2").arg(fnLyrics).arg(EXT_TEXT));
    qf.open(QIODevice::WriteOnly | QIODevice::Text);  // TODO: check for failure here!!!
    qts.setDevice(&qf);
    qts << lyricsHTML;

    // Write and close the HTML file
    qts.flush();
    qf.close();

    // The HTML lyrics' VTT file generation is pre-encapsulated, shared by frets.
    saveStartVTT(score, fnLyrics, &setVTT, 0);

    delete art; // the friendly thing to do

    return true;
}

bool MuseScore::saveSMAWS_Tour(Score* score, QFileInfo* qfi)
{
    if (score->metaTag(tagWorkNo).isEmpty()) {
        QMessageBox::critical(this, tr("SMAWS: saveSMAWS_Tour"),
            tr("You must set the Work Number property for this Score.\nUse File menu / Score Properties dialog."));
        return false;
    }

    Int2StrMap  mapVTT; // key = CueID, value = command list
    ChordRest*  cr;
    Measure*    m;
    Segment*    s;
    QString     qs;
    QString     description;
    QTextStream qts(&qs);
    Element*    eChapter;
    bool        hasDesc;
    bool        hasLyrics;

    //!!Does this ever need more than one staff??
    // By measure by segment of type ChordRest
    for (m = score->firstMeasure(); m; m = m->nextMeasureMM()) {
        for (s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            eChapter = 0;        // chapters defined by rehearsal marks
            hasDesc  = false;
            description.clear();
            for (Element* eAnn : s->annotations()) {
               if (eAnn->type() == EType::REHEARSAL_MARK) {
                   eChapter = eAnn;
               }
               if (eAnn->type() == EType::STAFF_TEXT) {
                   description = static_cast<Text*>(eAnn)->xmlText();
               }
               if (eChapter && !description.isEmpty()) {
                   hasDesc = true;
                   break;
               }
            }
            cr = s->cr(0); // one staff, one voice
            hasLyrics = (cr->lyrics().size() > 0);
            if (cr != 0 && (eChapter || hasLyrics)) {
                qs.clear();
                if (eChapter)
                    qts << "chapter:"
                        << static_cast<const Text*>(eChapter)->xmlText()
                        << (hasLyrics ? "," : "");
                if (hasLyrics)
                    qts << cr->lyrics()[0]->plainText();
                if (hasDesc)
                    qts << endl << description;
                mapVTT.insert(cr->tick(), qs);
            }
        }
    }

    // The End
    m = score->lastMeasureMM();
    mapVTT.insert(m->tick() + m->ticks(), "end");

    return saveStartVTT(score,
                        QString("%1/%2%3%4").arg(qfi->path())
                                            .arg(score->metaTag(tagWorkNo))
                                            .arg(SMAWS_)
                                            .arg(score->metaTag(tagMoveNo)),
                        0, &mapVTT);
}

// End SVG / SMAWS
///////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//   createThumbnail
//---------------------------------------------------------

static QPixmap createThumbnail(const QString& name)
      {
      if (!(name.endsWith(".mscx") || name.endsWith(".mscz")))
            return QPixmap();
      MasterScore* score = new MasterScore(MScore::defaultStyle());
      Score::FileError error = readScore(score, name, true);
      if (error != Score::FileError::FILE_NO_ERROR || !score->firstMeasure()) {
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

//---------------------------------------------------------
//   saveMetadataJSON
//---------------------------------------------------------

bool MuseScore::saveMetadataJSON(Score* score, const QString& name)
      {
      QFile f(name);
      if (!f.open(QIODevice::WriteOnly))
            return false;

      QJsonObject json = saveMetadataJSON(score);
      QJsonDocument saveDoc(json);
      f.write(saveDoc.toJson());
      f.close();
      return true;
      }

//---------------------------------------------------------
//   findTextByType
//    @data must contain std::pair<Tid, QStringList*>*
//          Tid specifies text style
//          QStringList* specifies the container to keep found text
//
//    For usage with Score::scanElements().
//    Finds all text elements with specified style.
//---------------------------------------------------------
static void findTextByType(void* data, Element* element)
      {
      if (!element->isTextBase())
            return;
      TextBase* text = toTextBase(element);
      auto* typeStringsData = static_cast<std::pair<Tid, QStringList*>*>(data);
      if (text->tid() == typeStringsData->first) {
            // or if score->getTextStyleUserName().contains("Title") ???
            // That is bad since it may be localized
            QStringList* titleStrings = typeStringsData->second;
            Q_ASSERT(titleStrings);
            titleStrings->append(text->plainText());
            }
      }
      
QJsonObject MuseScore::saveMetadataJSON(Score* score)
      {
      auto boolToString = [](bool b) { return b ? "true" : "false"; };
      QJsonObject json;

      // title
      QString title;
      Text* t = score->getText(Tid::TITLE);
      if (t)
            title = QTextDocumentFragment::fromHtml(t->xmlText()).toPlainText().replace("&amp;","&").replace("&gt;",">").replace("&lt;","<").replace("&quot;", "\"");
      if (title.isEmpty())
            title = score->metaTag("workTitle");
      if (title.isEmpty())
            title = score->title();
      title = title.simplified();
      json.insert("title", title);

      // subtitle
      QString subtitle;
      t = score->getText(Tid::SUBTITLE);
      if (t)
            subtitle = QTextDocumentFragment::fromHtml(t->xmlText()).toPlainText().replace("&amp;","&").replace("&gt;",">").replace("&lt;","<").replace("&quot;", "\"");
      subtitle = subtitle.simplified();
      json.insert("subtitle", subtitle);

      // composer
      QString composer;
      t = score->getText(Tid::COMPOSER);
      if (t)
            composer = QTextDocumentFragment::fromHtml(t->xmlText()).toPlainText().replace("&amp;","&").replace("&gt;",">").replace("&lt;","<").replace("&quot;", "\"");
      if (composer.isEmpty())
            composer = score->metaTag("composer");
      composer = composer.simplified();
      json.insert("composer", composer);

      // poet
      QString poet;
      t = score->getText(Tid::POET);
      if (t)
            poet = QTextDocumentFragment::fromHtml(t->xmlText()).toPlainText().replace("&amp;","&").replace("&gt;",">").replace("&lt;","<").replace("&quot;", "\"");
      if (poet.isEmpty())
            poet = score->metaTag("lyricist");
      poet = poet.simplified();
      json.insert("poet", poet);

      json.insert("mscoreVersion", score->mscoreVersion());
      json.insert("fileVersion", score->mscVersion());

      json.insert("pages", score->npages());
      json.insert("measures", score->nmeasures());
      json.insert("hasLyrics", boolToString(score->hasLyrics()));
      json.insert("hasHarmonies", boolToString(score->hasHarmonies()));
      json.insert("keysig", score->keysig());

      // timeSig
      QString timeSig;
      int staves = score->nstaves();
      int tracks = staves * VOICES;
      Segment* tss = score->firstSegmentMM(SegmentType::TimeSig);
      if (tss) {
            Element* e = nullptr;
            for (int track = 0; track < tracks; ++track) {
                  e = tss->element(track);
                  if (e) break;
                  }
            if (e && e->isTimeSig()) {
                  TimeSig* ts = toTimeSig(e);
                  timeSig = QString("%1/%2").arg(ts->numerator()).arg(ts->denominator());
                  }
            }
      json.insert("timesig", timeSig);

      json.insert("duration", score->duration());
      json.insert("lyrics", score->extractLyrics());

      // tempo
       int tempo = 0;
       QString tempoText;
       for (Segment* seg = score->firstSegmentMM(SegmentType::All); seg; seg = seg->next1MM()) {
             auto annotations = seg->annotations();
             for (Element* a : annotations) {
                   if (a && a->isTempoText()) {
                         TempoText* tt = toTempoText(a);
                         tempo = round(tt->tempo() * 60);
                         tempoText = tt->xmlText();
                         }
                   }
             }
      json.insert("tempo", tempo);
      json.insert("tempoText", tempoText);

      // parts
      QJsonArray jsonPartsArray;
      for (Part* p : score->parts()) {
            QJsonObject jsonPart;
            jsonPart.insert("name", p->longName().replace("\n", ""));
            int midiProgram = p->midiProgram();
            if (p->midiChannel() == 9)
                midiProgram = 128;
            jsonPart.insert("program", midiProgram);
            jsonPart.insert("instrumentId", p->instrumentId());
            jsonPart.insert("lyricCount", p->lyricCount());
            jsonPart.insert("harmonyCount", p->harmonyCount());
            jsonPart.insert("hasPitchedStaff", boolToString(p->hasPitchedStaff()));
            jsonPart.insert("hasTabStaff", boolToString(p->hasTabStaff()));
            jsonPart.insert("hasDrumStaff", boolToString(p->hasDrumStaff()));
            jsonPart.insert("isVisible", boolToString(p->show()));
            jsonPartsArray.append(jsonPart);
            }
      json.insert("parts", jsonPartsArray);

      // pageFormat
      QJsonObject jsonPageformat;
      QRectF      rect = score->style().pageOdd().fullRect(QPageLayout::Millimeter);
      jsonPageformat.insert("height",   round(rect.width() ));
      jsonPageformat.insert("width",    round(rect.height()));
      jsonPageformat.insert("twosided", boolToString(score->styleB(Sid::pageTwosided)));
      json.insert("pageFormat", jsonPageformat);
      
      //text frames metadata
      QJsonObject jsonTypeData;
      static std::vector<std::pair<QString, Tid>> namesTypesList {
            {"titles", Tid::TITLE},
            {"subtitles", Tid::SUBTITLE},
            {"composers", Tid::COMPOSER},
            {"poets", Tid::POET}
            };
      for (auto nameType : namesTypesList) {
            QJsonArray typeData;
            QStringList typeTextStrings;
            std::pair<Tid, QStringList*> extendedTitleData = std::make_pair(nameType.second, &typeTextStrings);
            score->scanElements(&extendedTitleData, findTextByType);
            for (auto typeStr : typeTextStrings)
                  typeData.append(typeStr);
            jsonTypeData.insert(nameType.first, typeData);
            }
      json.insert("textFramesData", jsonTypeData);
      
      return json;
      }

class CustomJsonWriter
{
public:
      CustomJsonWriter(const QString& filePath)
      {
      jsonFormatFile.setFileName(filePath);
      jsonFormatFile.open(QIODevice::WriteOnly);
      jsonFormatFile.write("{\n");
      }
      
      ~CustomJsonWriter()
      {
      jsonFormatFile.write("\n}\n");
      jsonFormatFile.close();
      }
      
      void addKey(const char* arrayName)
      {
      jsonFormatFile.write("\"");
      jsonFormatFile.write(arrayName);
      jsonFormatFile.write("\": ");
      }
      
      void addValue(const QByteArray& data, bool lastJsonElement = false, bool isJson = false)
      {
      if (!isJson)
            jsonFormatFile.write("\"");
      jsonFormatFile.write(data);
      if (!isJson)
            jsonFormatFile.write("\"");
      if (!lastJsonElement)
            jsonFormatFile.write(",\n");
      }
      
      void openArray()
      {
      jsonFormatFile.write(" [");
      }
      
      void closeArray(bool lastJsonElement = false)
      {
      jsonFormatFile.write("]");
      if (!lastJsonElement)
            jsonFormatFile.write(",");
      jsonFormatFile.write("\n");
      }
      
private:
      QFile jsonFormatFile;
};
      
//---------------------------------------------------------
//   exportMp3AsJSON
//---------------------------------------------------------

bool MuseScore::exportMp3AsJSON(const QString& inFilePath, const QString& outFilePath)
      {
      std::unique_ptr<MasterScore> score(mscore->readScore(inFilePath));
      if (!score)
            return false;

      CustomJsonWriter jsonWriter(outFilePath);
      jsonWriter.addKey("mp3");
      //export score audio
      QByteArray mp3Data;
      QBuffer mp3Device(&mp3Data);
      mp3Device.open(QIODevice::ReadWrite);
      bool dummy = false;
      mscore->saveMp3(score.get(), &mp3Device, dummy);
      jsonWriter.addValue(mp3Data.toBase64(), true);
      return true;
      }

QByteArray MuseScore::exportPdfAsJSON(Score* score)
      {
      QPrinter printer;
      auto tempPdfFileName = "/tmp/MUTempPdf.pdf";
      printer.setOutputFileName(tempPdfFileName);
      mscore->savePdf(score, printer);
      QFile tempPdfFile(tempPdfFileName);
      QByteArray pdfData;
      if (tempPdfFile.open(QIODevice::ReadWrite)) {
            pdfData = tempPdfFile.readAll();
            tempPdfFile.close();
            tempPdfFile.remove();
            }

      return pdfData.toBase64();
      }

//---------------------------------------------------------
//   exportAllMediaFiles
//---------------------------------------------------------

bool MuseScore::exportAllMediaFiles(const QString& inFilePath, const QString& outFilePath)
      {
      std::unique_ptr<MasterScore> score(mscore->readScore(inFilePath));
      if (!score)
            return false;

      score->switchToPageMode();

      //// JSON specification ///////////////////////////
      //jsonForMedia["pngs"] = pngsJsonArray;
      //jsonForMedia["mposXML"] = mposJson;
      //jsonForMedia["sposXML"] = sposJson;
      //jsonForMedia["pdf"] = pdfJson;
      //jsonForMedia["svgs"] = svgsJsonArray;
      //jsonForMedia["midi"] = midiJson;
      //jsonForMedia["mxml"] = mxmlJson;
      //jsonForMedia["metadata"] = mdJson;
      ///////////////////////////////////////////////////

      bool res = true;
      CustomJsonWriter jsonWriter(outFilePath);
      //export score pngs and svgs
      jsonWriter.addKey("pngs");
      jsonWriter.openArray();
      for (int i = 0; i < score->pages().size(); ++i) {
            QByteArray pngData;
            QBuffer pngDevice(&pngData);
            pngDevice.open(QIODevice::ReadWrite);
            res &= mscore->savePng(score.get(), &pngDevice, i);
            bool lastArrayValue = ((score->pages().size() - 1) == i);
            jsonWriter.addValue(pngData.toBase64(), lastArrayValue);
            }
      jsonWriter.closeArray();
      
      jsonWriter.addKey("svgs");
      jsonWriter.openArray();
      for (int i = 0; i < score->pages().size(); ++i) {
            QByteArray svgData;
            QBuffer svgDevice(&svgData);
            svgDevice.open(QIODevice::ReadWrite);
            res &= mscore->saveSvg(score.get(), &svgDevice, i);
            bool lastArrayValue = ((score->pages().size() - 1) == i);
            jsonWriter.addValue(svgData.toBase64(), lastArrayValue);
            }
      jsonWriter.closeArray();

      {
      //export score .spos
      QByteArray partDataPos;
      QBuffer partPosDevice(&partDataPos);
      partPosDevice.open(QIODevice::ReadWrite);
      savePositions(score.get(), &partPosDevice, true);
      jsonWriter.addKey("sposXML");
      jsonWriter.addValue(partDataPos.toBase64());
      partPosDevice.close();
      partDataPos.clear();
      
      //export score .mpos
      partPosDevice.open(QIODevice::ReadWrite);
      savePositions(score.get(), &partPosDevice, false);
      jsonWriter.addKey("mposXML");
      jsonWriter.addValue(partDataPos.toBase64());
      }
      
      //export score pdf
      jsonWriter.addKey("pdf");
      jsonWriter.addValue(exportPdfAsJSON(score.get()));

      {
      //export score midi
      QByteArray midiData;
      QBuffer midiDevice(&midiData);
      midiDevice.open(QIODevice::ReadWrite);
      res &= mscore->saveMidi(score.get(), &midiDevice);
      jsonWriter.addKey("midi");
      jsonWriter.addValue(midiData.toBase64());
      }
      
      {
      //export musicxml
      QByteArray mxmlData;
      QBuffer mxmlDevice(&mxmlData);
      mxmlDevice.open(QIODevice::ReadWrite);
      res &= saveMxl(score.get(), &mxmlDevice);
      jsonWriter.addKey("mxml");
      jsonWriter.addValue(mxmlData.toBase64());
      }
      
      //export metadata
      QJsonDocument doc(mscore->saveMetadataJSON(score.get()));
      jsonWriter.addKey("metadata");
      jsonWriter.addValue(doc.toJson(QJsonDocument::Compact), true, true);

      return res;
      }

}

