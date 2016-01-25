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

// SMAWS includes and defines
#include "libmscore/tempo.h"
#include "libmscore/chord.h"
#include "libmscore/notedot.h"
#include "libmscore/accidental.h"

// For QFileDialog. See MuseScore::exportFile() below
#define EXT_SVG ".svg"
#define EXT_VTT ".vtt"
#define FILTER_SMAWS        "SMAWS SVG+VTT"
#define FILTER_SMAWS_RULERS "SMAWS Rulers"

// For Cue ID formatting
#define CUE_ID_FIELD_WIDTH  7

// For Frozen Pane formatting
#define WIDTH_CLEF     16
#define WIDTH_KEY_SIG   5
#define WIDTH_TIME_SIG 10
#define X_OFF_TIME_SIG  3
// SMAWS end

extern Ms::Score::FileError importOve(Ms::Score*, const QString& name);

namespace Ms {

extern Score::FileError importMidi(Score*, const QString& name);
extern Score::FileError importGTP(Score*, const QString& name);
extern Score::FileError importBww(Score*, const QString& path);
extern Score::FileError importMusicXml(Score*, const QString&);
extern Score::FileError importCompressedMusicXml(Score*, const QString&);
extern Score::FileError importMuseData(Score*, const QString& name);
extern Score::FileError importLilypond(Score*, const QString& name);
extern Score::FileError importBB(Score*, const QString& name);
extern Score::FileError importCapella(Score*, const QString& name);
extern Score::FileError importCapXml(Score*, const QString& name);

extern Score::FileError readScore(Score* score, QString name, bool ignoreVersionError);

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

static void paintElements(QPainter& p, const QList<const Element*>& el)
{
    foreach (const Element* e, el) {
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
      fn = fn.replace(QChar(0xe4), "ae");
      fn = fn.replace(QChar(0xf6), "oe");
      fn = fn.replace(QChar(0xfc), "ue");
      fn = fn.replace(QChar(0xdf), "ss");
      fn = fn.replace(QChar(0xc4), "Ae");
      fn = fn.replace(QChar(0xd6), "Oe");
      fn = fn.replace(QChar(0xdc), "Ue");
      fn = fn.replace( QRegExp( "[" + QRegExp::escape( "\\/:*?\"<>|" ) + "]" ), "_" ); //FAT/NTFS special chars
      return fn;
      }

//---------------------------------------------------------
//   readScoreError
//    if "ask" is true, ask to ignore; returns true if
//    ignore is pressed by user
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
                  msg += QObject::tr("It was last saved with version 0.9.5 or older.<br>"
                         "You can convert this score by opening and then saving with"
                         " MuseScore version 1.x</a>");
                  canIgnore = true;
                  break;
            case Score::FileError::FILE_TOO_NEW:
                  msg += QObject::tr("This score was saved using a newer version of MuseScore.<br>\n"
                         "Visit the <a href=\"http://musescore.org\">MuseScore website</a>"
                         " to obtain the latest version.");
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
      int rv = false;
      if (converterMode || pluginMode) {
            fprintf(stderr, "%s\n", qPrintable(msg));
            return rv;
            }
      QMessageBox msgBox;
      msgBox.setWindowTitle(QObject::tr("MuseScore: Load Error"));
      msgBox.setText(msg);
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
      return rv;
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
               "before closing?").arg(s->name()),
               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
               QMessageBox::Save);
            if (n == QMessageBox::Save) {
                  if (s->isSavable()) {
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
         tr("All Supported Files (*.mscz *.mscx *.xml *.mxl *.mid *.midi *.kar *.md *.mgu *.MGU *.sgu *.SGU *.cap *.capx *.pdf *.ove *.scw *.bww *.GTP *.GP3 *.GP4 *.GP5 *.GPX);;")+
#else
         tr("All Supported Files (*.mscz *.mscx *.xml *.mxl *.mid *.midi *.kar *.md *.mgu *.MGU *.sgu *.SGU *.cap *.capx *.ove *.scw *.bww *.GTP *.GP3 *.GP4 *.GP5 *.GPX);;")+
#endif
         tr("MuseScore Files (*.mscz *.mscx);;")+
         tr("MusicXML Files (*.xml *.mxl);;")+
         tr("MIDI Files (*.mid *.midi *.kar);;")+
         tr("Muse Data Files (*.md);;")+
         tr("Capella Files (*.cap *.capx);;")+
         tr("BB Files <experimental> (*.mgu *.MGU *.sgu *.SGU);;")+
#ifdef OMR
         tr("PDF Files <experimental OMR> (*.pdf);;")+
#endif
         tr("Overture / Score Writer Files <experimental> (*.ove *.scw);;")+
         tr("Bagpipe Music Writer Files <experimental> (*.bww);;")+
         tr("Guitar Pro (*.GTP *.GP3 *.GP4 *.GP5 *.GPX)"),
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
            if (s->fileInfo()->canonicalFilePath() == path)
                  return 0;
            }

      Score* score = readScore(fn);
      if (score) {
            setCurrentScoreView(appendScore(score));
            writeSessionFile(false);
            }
      return score;
      }

//---------------------------------------------------------
//   readScore
//---------------------------------------------------------

Score* MuseScore::readScore(const QString& name)
      {
      if (name.isEmpty())
            return 0;

      Score* score = new Score(MScore::baseStyle());  // start with built-in style
      setMidiReopenInProgress(name);
      Score::FileError rv = Ms::readScore(score, name, false);
      if (rv == Score::FileError::FILE_TOO_OLD || rv == Score::FileError::FILE_TOO_NEW || rv == Score::FileError::FILE_CORRUPTED) {
            if (readScoreError(name, rv, true)) {
                  delete score;
                  score = new Score(MScore::baseStyle());
                  rv = Ms::readScore(score, name, true);
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
      return saveFile(cs->rootScore());
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
            QString fn = score->fileInfo()->fileName();
            Text* t = score->getText(TextStyleType::TITLE);
            if (t)
                  fn = t->plainText(true);
            QString name = createDefaultFileName(fn);
            QString f1 = tr("MuseScore File (*.mscz)");
            QString f2 = tr("Uncompressed MuseScore File (*.mscx)");

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
            score->fileInfo()->setFile(fn);

            mscore->lastSaveDirectory = score->fileInfo()->absolutePath();

            if (!score->saveFile()) {
                  QMessageBox::critical(mscore, tr("MuseScore: Save File"), MScore::lastError);
                  return false;
                  }
            addRecentScore(score);
            writeSessionFile(false);
            }
      else if (!score->saveFile()) {
            QMessageBox::critical(mscore, tr("MuseScore: Save File"), MScore::lastError);
            return false;
            }
      score->setCreated(false);
      setWindowTitle("MuseScore: " + score->name());
      int idx = scoreList.indexOf(score);
      tab1->setTabText(idx, score->name());
      if (tab2)
            tab2->setTabText(idx, score->name());
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
                  if (s->name() == tmpName) {
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

      Score* score;
      QString tp = newWizard->templatePath();

      QList<Excerpt*> excerpts;
      if (!newWizard->emptyScore()) {
            Score* tscore = new Score(MScore::defaultStyle());
            Score::FileError rv = Ms::readScore(tscore, tp, false);
            if (rv != Score::FileError::FILE_NO_ERROR) {
                  readScoreError(newWizard->templatePath(), rv, false);
                  delete tscore;
                  return;
                  }
            score = new Score(tscore->style());
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
            score = new Score(MScore::defaultStyle());
            newWizard->createInstruments(score);
            }
      score->setCreated(true);
      score->fileInfo()->setFile(createDefaultName());

      if (!score->style()->chordList()->loaded()) {
            if (score->style()->value(StyleIdx::chordsXmlFile).toBool())
                  score->style()->chordList()->read("chords.xml");
            score->style()->chordList()->read(score->style()->value(StyleIdx::chordDescriptionFile).toString());
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
                              QList<TDuration> dList = toDurationList(measure->len(), false);
                              if (!dList.isEmpty()) {
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
      score->lastMeasure()->setEndBarLineType(BarLineType::END, false);

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
            Score* xs = new Score(score);
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
                     tr("MuseScore Styles (*.mss)")
                     );
                  }
            else {
                  fn = QFileDialog::getSaveFileName(
                     this, tr("MuseScore: Save Style"),
                     defaultPath,
                     tr("MuseScore Style File (*.mss)")
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
                  loadStyleDialog->setNameFilter(tr("MuseScore Style File (*.mss)"));
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
                  saveStyleDialog->setNameFilter(tr("MuseScore Style File (*.mss)"));
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
      QString filter = tr("Chord Symbols Style File (*.xml)");

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
      QString filter = tr("PDF Scan File (*.pdf);;All (*)");
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
      QString filter = tr("Ogg Audio File (*.ogg);;All (*)");
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
      QString wd      = QString("%1/%2").arg(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation)).arg(QCoreApplication::applicationName());
      if (open) {
            title  = tr("MuseScore: Load Palette");
            filter = tr("MuseScore Palette (*.mpal)");
            }
      else {
            title  = tr("MuseScore: Save Palette");
            filter = tr("MuseScore Palette (*.mpal)");
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
            filter = tr("MuseScore Plugin (*.qml)");
            }
      else {
            title  = tr("MuseScore: Save Plugin");
            filter = tr("MuseScore Plugin File (*.qml)");
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
            filter = tr("MuseScore Drumset (*.drm)");
            }
      else {
            title  = tr("MuseScore: Save Drumset");
            filter = tr("MuseScore Drumset File (*.drm)");
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
      printerDev.setPaperSize(pf->size(), QPrinter::Point);

      printerDev.setCreator("MuseScore Version: " VERSION);
      printerDev.setFullPage(true);
      if (!printerDev.setPageMargins(QMarginsF()))
            qDebug("unable to clear printer margins");
      printerDev.setColorMode(QPrinter::Color);
      printerDev.setDocName(cs->name());
      printerDev.setOutputFormat(QPrinter::NativeFormat);
      printerDev.setFromTo(1, cs->pages().size());

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
      printerDev.setOutputFileName("");
#else
      // when setting this on windows platform, pd.exec() does not
      // show dialog
      printerDev.setOutputFileName(cs->fileInfo()->path() + "/" + cs->name() + ".pdf");
#endif

      QPrintDialog pd(&printerDev, 0);

      if (!pd.exec())
            return;

      LayoutMode layoutMode = cs->layoutMode();
      if (layoutMode != LayoutMode::PAGE) {
            cs->startCmd();
            cs->ScoreElement::undoChangeProperty(P_ID::LAYOUT_MODE, int(LayoutMode::PAGE));
            cs->doLayout();
            }

      QPainter p(&printerDev);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      double mag = printerDev.logicalDpiX() / DPI;

      p.scale(mag, mag);

      const QList<Page*> pl = cs->pages();
      int pages    = pl.size();
      int offset   = cs->pageNumberOffset();
      int fromPage = printerDev.fromPage() - 1 - offset;
      int toPage   = printerDev.toPage() - 1 - offset;
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
      if (layoutMode != cs->layoutMode())
            cs->endCmd(true);       // rollback
      }

//---------------------------------------------------------
//   exportFile
//    return true on success
//---------------------------------------------------------

void MuseScore::exportFile()
      {
      QStringList fl;
      fl.append(tr("PDF File (*.pdf)"));
      fl.append(tr("PNG Bitmap Graphic (*.png)"));
      fl.append(tr("Scalable Vector Graphic (*%1)").arg(EXT_SVG));
#ifdef HAS_AUDIOFILE
      fl.append(tr("Wave Audio (*.wav)"));
      fl.append(tr("FLAC Audio (*.flac)"));
      fl.append(tr("Ogg Vorbis Audio (*.ogg)"));
#endif
#ifdef USE_LAME
      fl.append(tr("MP3 Audio (*.mp3)"));
#endif
      fl.append(tr("Standard MIDI File (*.mid)"));
      fl.append(tr("MusicXML File (*.xml)"));
      fl.append(tr("Compressed MusicXML File (*.mxl)"));
      fl.append(tr("Uncompressed MuseScore File (*.mscx)"));
// SMAWS options
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS).arg(EXT_VTT));
      fl.append(tr("%1 (*%2)").arg(FILTER_SMAWS_RULERS).arg(EXT_VTT));
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
            if (cs->parentScore())
                  name = QString("%1/%2-%3").arg(saveDirectory).arg(cs->parentScore()->name()).arg(createDefaultFileName(cs->name()));
            else
                  name = QString("%1/%2").arg(saveDirectory).arg(cs->name());
            }
      else
#endif
      if (cs->parentScore())
            name = QString("%1/%2-%3.%4").arg(saveDirectory).arg(cs->parentScore()->name()).arg(createDefaultFileName(cs->name())).arg(saveFormat);
      else
            name = QString("%1/%2.%3").arg(saveDirectory).arg(cs->name()).arg(saveFormat);

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

      // A bit of a kludge, reusing the saveCopy arg in MuseScore::saveAs().
      // It distinguishes between SMAWS and SMAWS_RULERS without adding an arg
      // to saveAs(), which is called in several places outside this file too.
      // The default prior to this variable existing, was true.
      bool saveCopy = true;
      if (selectedFilter.contains(FILTER_SMAWS_RULERS))
            saveCopy = false;

      if (fi.suffix().isEmpty())
            QMessageBox::critical(this, tr("MuseScore: Export"), tr("Cannot determine file type"));
      else
            saveAs(cs, saveCopy, fn, fi.suffix());
      }

//---------------------------------------------------------
//   exportParts
//    return true on success
//---------------------------------------------------------

bool MuseScore::exportParts()
      {
      QStringList fl;
      fl.append(tr("PDF File (*.pdf)"));
      fl.append(tr("PNG Bitmap Graphic (*.png)"));
      fl.append(tr("Scalable Vector Graphic (*%1)").arg(EXT_SVG));
#ifdef HAS_AUDIOFILE
      fl.append(tr("Wave Audio (*.wav)"));
      fl.append(tr("FLAC Audio (*.flac)"));
      fl.append(tr("Ogg Vorbis Audio (*.ogg)"));
#endif
#ifdef USE_LAME
      fl.append(tr("MP3 Audio (*.mp3)"));
#endif
      fl.append(tr("Standard MIDI File (*.mid)"));
      fl.append(tr("MusicXML File (*.xml)"));
      fl.append(tr("Compressed MusicXML File (*.mxl)"));
      fl.append(tr("MuseScore File (*.mscz)"));
      fl.append(tr("Uncompressed MuseScore File (*.mscx)"));

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

      QString scoreName = cs->parentScore() ? cs->parentScore()->name() : cs->name();
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

      Score* thisScore = cs->rootScore();
      bool overwrite = false;
      bool noToAll = false;
      QString confirmReplaceTitle = tr("Confirm Replace");
      QString confirmReplaceMessage = tr("\"%1\" already exists.\nDo you want to replace it?\n");
      QString replaceMessage = tr("Replace");
      QString skipMessage = tr("Skip");
      foreach (Excerpt* e, thisScore->excerpts())  {
            Score* pScore = e->partScore();
            QString partfn = fi.absolutePath() + QDir::separator() + fi.baseName() + "-" + createDefaultFileName(pScore->name()) + "." + ext;
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
            QString partfn(fi.absolutePath() + QDir::separator() + fi.baseName() + "-" + createDefaultFileName(tr("Score_and_Parts")) + ".pdf");
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
      if (ext == "mscx" || ext == "mscz") {
            // save as mscore *.msc[xz] file
            QFileInfo fi(fn);
            rv = true;
            // store new file and path into score fileInfo
            // to have it accessible to resources
            QString originalScoreFName(cs->fileInfo()->canonicalFilePath());
            cs->fileInfo()->setFile(fn);
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
            cs->fileInfo()->setFile(originalScoreFName);          // restore original file name

            if (rv && !saveCopy) {
                  cs->fileInfo()->setFile(fn);
                  setWindowTitle("MuseScore: " + cs->name());
                  cs->undo()->setClean();
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
            cs->switchToPageMode();
            rv = savePdf(cs, fn);
            }
      else if (ext == "png") {
            // save as png file *.png
            cs->switchToPageMode();
            rv = savePng(cs, fn);
            }
      else if (ext == "svg") {
            // save as svg file *.svg
            cs->switchToPageMode();
            rv = saveSvg(cs, fn);
            }
      else if (suffix == EXT_VTT) {
            // SMAWS: matched SVG + WebVTT files
            // Rulers are global to a composition, shared by all the parts
            // Users must generate them separately, once per composition.
            // It is both a convenience and a requirement if you want rulers.
            // This reuses the saveCopy arg in order to avoid another overload.
            // See MuseScore::exportFile() above.
            cs->switchToPageMode();
            if (saveCopy)
                  rv = saveSMAWS(cs, fn);
            else
                  rv = saveSMAWS_Rulers(cs, fn);
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
            cs->switchToPageMode();
            // save positions of segments
            rv = savePositions(cs, fn, true);
            }
      else if (ext == "mpos") {
            cs->switchToPageMode();
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
      if (layoutMode != cs->layoutMode())
            cs->endCmd(true);       // rollback
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
      printerDev.setTitle(cs->name());

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
      printerDev.setDocName(firstScore->name());
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
            s->switchToPageMode();
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

            if (layoutMode != s->layoutMode())
                  s->endCmd(true);       // rollback
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
            QStringList pl = preferences.sfPath.split(";");
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
                          QMessageBox::Yes|QMessageBox::No, QMessageBox::NoButton);
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

Score::FileError readScore(Score* score, QString name, bool ignoreVersionError)
      {
      QFileInfo info(name);
      QString suffix  = info.suffix().toLower();
      score->setName(info.completeBaseName());
      score->setImportedFilePath(name);

      if (suffix == "mscz" || suffix == "mscx") {
            Score::FileError rv = score->loadMsc(name, ignoreVersionError);
            if (score && score->fileInfo()->path().startsWith(":/"))
                  score->setCreated(true);
            if (rv != Score::FileError::FILE_NO_ERROR)
                  return rv;
            }
      else if (suffix == "sf2" || suffix == "sf3") {
            importSoundfont(name);
            return Score::FileError::FILE_IGNORE_ERROR;
            }
      else {
            // typedef Score::FileError (*ImportFunction)(Score*, const QString&);
            struct ImportDef {
                  const char* extension;
                  // ImportFunction importF;
                  Score::FileError (*importF)(Score*, const QString&);
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

      score->setLayoutAll(true);
      for (Score* s : score->scoreList()) {
            s->setPlaylistDirty();
            s->rebuildMidiMapping();
            s->updateChannel();
            s->setSoloMute();
            s->addLayoutFlags(LayoutFlag::FIX_TICKS | LayoutFlag::FIX_PITCH_VELO);
            }
      score->setSaved(false);
      score->update();
      if (!ignoreVersionError && !MScore::noGui)
            if (!score->sanityCheck())
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
      fl.append(tr("MuseScore File (*.mscz)"));
      fl.append(tr("Uncompressed MuseScore File (*.mscx)"));     // for debugging purposes
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
            if (cs->parentScore())
                  name = QString("%1/%2-%3").arg(saveDirectory).arg(cs->parentScore()->name()).arg(createDefaultFileName(cs->name()));
            else
                  name = QString("%1/%2").arg(saveDirectory).arg(cs->name());
            }
      else
#endif
      if (cs->parentScore())
            name = QString("%1/%2-%3.mscz").arg(saveDirectory).arg(cs->parentScore()->name()).arg(createDefaultFileName(cs->name()));
      else
            name = QString("%1/%2.mscz").arg(saveDirectory).arg(cs->name());

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
      fl.append(tr("MuseScore File (*.mscz)"));
      QString saveDialogTitle = tr("MuseScore: Save Selection");

      QSettings settings;
      if (mscore->lastSaveDirectory.isEmpty())
            mscore->lastSaveDirectory = settings.value("lastSaveDirectory", preferences.myScoresPath).toString();
      QString saveDirectory = mscore->lastSaveDirectory;

      if (saveDirectory.isEmpty())
            saveDirectory = preferences.myScoresPath;

      QString name   = QString("%1/%2.mscz").arg(saveDirectory).arg(cs->name());
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
         tr("All Supported Files (*.svg *.jpg *.jpeg *.png);;"
            "Scalable Vector Graphics (*.svg);;"
            "JPEG (*.jpg *.jpeg);;"
            "PNG (*.png)"
            )
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

            QList<const Element*> pel = page->elements();
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
      cs->setPrinting(false);
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
      QString filter = tr("Images (*.jpg *.jpeg *.png);;All (*)");
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
//   saveSMAWS()       writes linked SVG and VTT files for animation
//   saveVTT()         a helper function exclusively for saveSMAWS()
//   getCueID()        another little helper (Cue IDs link SVG to VTT)
//   getAnnCueID()     gets Annotation Cue ID (Harmony and RehearsalMark)
//   getScrollCueID()  gets Cue ID for scrolling cues and RehearsalMarks
///////////////////////////////////////////////////////////////////////////////
// paintStaffLines() - consolidates shared code in saveSVG() and saveSMAWS()
//                     for MuseScore master, no harm, no gain, 100% neutral
static void paintStaffLines(Score* score, Page* page, QPainter* p, SvgGenerator* printer)
{
    foreach (System* s, *(page->systems())) {
        for (int i = 0, n = s->staves()->size(); i < n; i++) {
            if (score->staff(i)->invisible())
                continue;  // ignore invisible staves

            // The goal here is to draw SVG staff lines more efficiently.
            // MuseScore draws staff lines by measure, but for SVG they can
            // generally be drawn once for each system. This makes a big
            // difference for scores that scroll horizontally on a single
            // page. But there is an exception to this rule:
            //
            //   ~ One (or more) invisible measure(s) in a system/staff ~
            //     In this case the SVG staff lines for the system/staff
            //     are drawn by measure.
            //
            bool byMeasure = false;
            for (MeasureBase* mb = s->firstMeasure(); mb != 0; mb = s->nextMeasure(mb)) {
                if (!static_cast<Measure*>(mb)->visible(i)) {
                    byMeasure = true;
                    break;
                }
            }
            if (byMeasure) { // Draw visible staff lines by measure
                for (MeasureBase* mb = s->firstMeasure(); mb != 0; mb = s->nextMeasure(mb)) {
                    Measure* m = static_cast<Measure*>(mb);
                    if (m->visible(i)) {
                        StaffLines* sl = m->staffLines(i);
                        printer->setElement(sl);
                        paintElement(*p, sl);
                    }
                }
            }
            else { // Draw staff lines once per system
                StaffLines* firstSL = s->firstMeasure()->staffLines(i)->clone();
                StaffLines*  lastSL =  s->lastMeasure()->staffLines(i);
                firstSL->bbox().setRight(lastSL->bbox().right()
                                      +  lastSL->pagePos().x()
                                      - firstSL->pagePos().x());
                printer->setElement(firstSL);
                paintElement(*p, firstSL);
            }
        } // for each Staff
    } //for each System
}
// svgInit() - consolidates shared code in saveSVG and saveSMAWS.
//             for MuseScore master, no harm, no gain, 100% neutral
static bool svgInit(Score* score,
                    const QString& saveName,
                    SvgGenerator* printer,
                    QPainter* p,
                    bool bSMAWS = false)
{
    printer->setFileName(saveName);
    printer->setTitle(score->title());

    const PageFormat* pf = score->pageFormat();
    QRectF r;
    if (trimMargin >= 0 && score->npages() == 1) {
          QMarginsF margins(trimMargin, trimMargin, trimMargin, trimMargin);
          r = score->pages().first()->tbbox() + margins;
          }
    else
          r = QRectF(0, 0, pf->width() * score->pages().size(), pf->height());
    qreal w = r.width();
    qreal h = r.height();

    printer->setViewBox(QRectF(0, 0, w, h));
    if (bSMAWS)
        // SMAWS sets an initial 200% zoom
        printer->setSize(QSize(w * 2, h * 2));
    else
        printer->setSize(QSize(w, h));

    score->setPrinting(true);
    MScore::pdfPrinting = true;

    if (!p->begin(printer))
        return false;

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    if (trimMargin >= 0 && score->npages() == 1)
          p->translate(-r.topLeft());

    return true;
}
// MuseScore::saveSvg() - This version is compatible with MuseScore master if
//                        svgInit() + paintStaffLines() are integrated as well.
bool MuseScore::saveSvg(Score* score, const QString& saveName)
{
      SvgGenerator printer;
      QPainter p;
      if (!svgInit(score, saveName, &printer, &p))
          return false;

      foreach (Page* page, score->pages()) {
        // 1st pass: StaffLines
        paintStaffLines(score, page, &p, &printer);

        // 2nd pass: the rest of the elements
        QList<const Element*> pel = page->elements();
        qStableSort(pel.begin(), pel.end(), elementLessThan);

        Element::Type eType;
        foreach (const Element* e, pel) {
            // Always exclude invisible elements
            if (!e->visible())
                    continue;

            eType = e->type();
            switch (eType) { // In future sub-type code, this switch() grows, and eType gets used
            case Element::Type::STAFF_LINES : // Handled in the 1st pass above
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
// 3 Cue ID generators: getCueID(), getAnnCueID(), getScrollCueID()
//
// getCueID()
// Creates a cue ID string from a start/end tick value pair
static QString getCueID(int startTick, int endTick = -1)
{
    // For cue_id formatting: 1234567_0000007
    const int   base       = 10;
    const QChar fillChar   = '0';

    // Missing endTick means zero duration cue, only one value required
    if (endTick < 0)
        endTick = startTick;

    return QString("%1_%2").arg(startTick, CUE_ID_FIELD_WIDTH, base, fillChar)
                             .arg(endTick, CUE_ID_FIELD_WIDTH, base, fillChar);
}
// getAnnCueID()
// Gets the cue ID for an annotation, such as rehearsal mark or chord symbol
static QString getAnnCueID(Score* score, Segment* segStart, Element::Type eType)
{
    int  startTick = segStart->tick();
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
//  - A ruler element: BarLine or RehearsalMark
//  - A frozen pane element: Clef, KeySig, or TimeSig
static QString getScrollCueID(Score* score, const Element* e)
{
    QString  cue_id = "";
    Element* p;
    Element::Type eType = e->type();

    // Always exclude invisible elements, except TEMPO_TEXT.
    if (!e->visible() && eType != Element::Type::TEMPO_TEXT)
        return cue_id;

    switch (eType) {
    case Element::Type::BAR_LINE       :
    case Element::Type::REHEARSAL_MARK :
    case Element::Type::CLEF           :
    case Element::Type::KEYSIG         :
    case Element::Type::TIMESIG        :
    case Element::Type::TEMPO_TEXT     :
    // Create the MuseScore and SMAWS-Rulers cue_id values:
    // There are N + 1 BarLines per System
    //     where N = number-of-measures-in-this-system
    //    (# of barlines/system is variable, not fixed)
    // Each Measure has only one BarLine, at its right edge (end-of-measure)
    // The first BarLine in each system is a System BarLine (parent() == System)
    // BarLine::tick() handles parent() == Segment, but not System. ...Maybe this should be changed there, but I don't want to open an additional module.
    // End-of-measure BarLine's parent is a Segment in the Measure.    Maybe RehearsalMark::tick() should be created too...
        p = e->parent();
        switch (p->type()) {
        case Element::Type::SYSTEM  :
        // System BarLines only used for scrolling = zero duration
        // System::firstMeasure() has the tick we need
            cue_id = getCueID(static_cast<System*>(p)->firstMeasure()->tick());
            break;
        case Element::Type::SEGMENT :
        // Measure BarLines are also zero duration, used for scrolling
        // and the rulers' playback position cursors.
        // RehearsalMarks only animate in the ruler, not in the score,
        // and they have full-duration cues, marker-to-marker.
            switch (eType) {
            case Element::Type::BAR_LINE :
                cue_id = getCueID(static_cast<Measure*>(p->parent())->tick());
                break;
            case Element::Type::REHEARSAL_MARK :
                cue_id = getAnnCueID(score, static_cast<Segment*>(p), eType);
                break;
            default: // Clefs, KeySigs, TimeSigs, TempoTexts
                cue_id = getCueID(static_cast<Segment*>(p)->tick());
                break;
            }
            break;
        default:
            break; // Should never happen
        }
        break;
    default: // Non-scrolling element types return an empty cue_id
        break;
    }

    return cue_id;
}

//
// 3 SMAWS file generators: saveVTT(), saveSMAWS_Rulers(), saveSMAWS()
//

// saveVTT()
// Private, static function called by saveSMAWS() (...and saveSMAWS_Rulers()?)
// Generates the WebVTT file (.vtt) using the setVTT arg as the data source.
static bool saveVTT(Score*              score,
                    const QString&      fileRoot,
                    QStringList&        setVTT)
{
    // This procedure opens files in only one mode
    const QIODevice::OpenMode openMode = QIODevice::WriteOnly | QIODevice::Text;

    // Open a stream into the file
    QFile fileVTT;
    fileVTT.setFileName(QString("%1%2").arg(fileRoot).arg(EXT_VTT));
    fileVTT.open(openMode);  // TODO: check for failure here!!!
    QTextStream streamVTT(&fileVTT);

    // Stream the header
    streamVTT << "WEBVTT\n\nNOTE\n"
              << "    SMAWS - Sheet Music Animation w/Sound\n"
              << "    This file links to thisfile.svg via cue id.\n"
              << "    For example: 0000000_1234567\nNOTE\n\n";
\
    // Change setVTT into a sorted set of unique cue IDs, then iterate over it
    setVTT.removeDuplicates();
    setVTT.sort();
    for (int i = 0, n = setVTT.size(); i < n; i++) {
        // Calculate start/end times (VTT minimum duration = 1ms, 00:00:00.001)
        TempoMap* tempos = score->tempomap();
        QString   cue_id = setVTT[i];
        int    startTick = cue_id.left(CUE_ID_FIELD_WIDTH).toInt();
        int      endTick = cue_id.right(CUE_ID_FIELD_WIDTH).toInt();
        QTime  startTime = startTime.fromMSecsSinceStartOfDay(qRound(tempos->tick2time(startTick) * 1000));
        QTime    endTime =   endTime.fromMSecsSinceStartOfDay(
                                      qRound(tempos->tick2time(endTick) * 1000)
                                    + (startTick == endTick ? 1 : 0)
                                     );
        // Stream the cue: 0000000_1234567
        //                 00:00:00.000 --> 12:34:56.789
        //                [this line intentionally left blank, per WebVTT spec]
        streamVTT << cue_id << endl
                  << QString("%1 --> %2\n\n")
                    .arg(startTime.toString("hh:mm:ss.zzz"))
                      .arg(endTime.toString("hh:mm:ss.zzz"));
    }
    // Write and close the VTT file
    streamVTT.flush();
    fileVTT.close();
    return true;
}

// MuseScore::saveSMAWS_Rulers()
// Generates the BarLine and RehearsalMarker rulers for the playback timeline.
// Currently this only generates two SVG files, zero VTT files. But a separate
// file for the ruler cues might be useful in the future...
bool MuseScore::saveSMAWS_Rulers(Score* score, const QString& saveName)
{
    // This is a long, tedious function, with lots of constant values.
    // The SVG is two files, generated once per composition by this function.
    // The VTT is generated by saveSMAWS() for each and every part and score
    // generated for that composition.
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

    // Constants and Variables:
    // Strings for the ruler's <line> and <text> element attributes
    // Some constants contain fixed values, others are just "attribute="
    QString y, anchor, label;
    const QString x           = " x= \"";
    const QString x1          = " x1=\"";
    const QString x2          = " x2=\"";
    const QString yBars       = " y=\"18\"";
    const QString yMarks      = " y=\"17\"";
    const QString y1          = " y1=\"";
    const QString y2          = " y2=\"";
    const QString data_cue    = " data-cue=\"";
    const QString anchorStart = " style=\"text-anchor:start\"";
    const QString anchorEnd   = " style=\"text-anchor:end\"";
    const QString openLine    = "<line";
    const QString openText    = "<text";
    const QString closeLine   = "/>";
    const QString closeText   = "</text>";
    const QString closer      = ">";
    const QString quote       = "\"";
    const QString endSVG      = "</svg>";
    const QString hdrBars     = "<?xml-stylesheet type=\"text/css\" href=\"SMAWS_21.css\" ?>\n<svg width=\"1360\" height=\"20\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    const QString hdrMarks    = "<?xml-stylesheet type=\"text/css\" href=\"SMAWS_21.css\" ?>\n<svg width=\"1360\" height=\"20\" xmlns=\"http://www.w3.org/2000/svg\"\n data-attr=\"fill\" data-hi=\"#00bb00\" data-lo=\"black\">\n";
    const QString border      = "<rect class=\"border\" x=\"0.5\" y=\"0.5\" width=\"1359\" height=\"19\"/>\n";
    const QString cursorBars  = "<polygon class=\"cursor\" points=\"-6,1 6,1 0,12\" transform=\"translate(8,0)\"/>\n";
    const QString cursorMarks = "<polygon class=\"cursor\" points=\"-6,19 6,19 0,7\" transform=\"translate(8,0)\"/>\n";
    // For CSS Styling
    QString classVal;
    const QString cssClass    = " class=\"";
    const QString classRul    = "ruler\" ";
    const QString classRul5   = "ruler5\"";
    const QString classMrkr   = "marker\"";
    // This is because of problems with specifying this value in CSS
    const QString stroke      = " style=\"stroke:black\"";
    // Default width of rulers = 1600 - 240 for counters (1600x900 = 16:9)
    const int wRuler = 1360;
    // Ruler lines don't start at zero or end at wRuler, left=right margin
    const int margin = 8;
    // Score::duration() returns # of seconds as an int, I need more accuracy
    const TempoMap* tempos = score->tempomap();
    const qreal duration = tempos->tick2time(score->lastMeasure()->tick()
                                           + score->lastMeasure()->ticks());
    // Pixels of width per second
    const qreal pxPerSec = (wRuler - (2 * margin)) / duration;
    // This procedure opens files in only one mode
    const QIODevice::OpenMode openMode = QIODevice::WriteOnly | QIODevice::Text;
    // The root file name, without the .ext
    const QString fileRoot = saveName.left(saveName.size() - 4);

    qreal   pxX;      // Floating point version of x coordinates
    qreal   offX;     // x offset for rehearsal mark text
    int     iY1, iY2; // Integer version of y1 and y2 coordinates
    int     iBarNo;   // Integer version of measure number
    QString fn;       // file name

    // 2 rulers = 2 SVG files
    QFile fileBars;  // Bar|Beat, but only bars in the ruler. Beats in a counter?
    QFile fileMarks; // Rehearsal marks
    // Open the files, setup the streams
    fn = QString("%1_%2%3").arg(fileRoot).arg("bars").arg(EXT_SVG);
    fileBars.setFileName(fn);
    fileBars.open(openMode);  // TODO: check for failure here!!!
    QTextStream streamBars(&fileBars);
    fn = QString("%1_%2%3").arg(fileRoot).arg("mrks").arg(EXT_SVG);
    fileMarks.setFileName(fn);
    fileMarks.open(openMode);  // TODO: check for failure here!!!
    QTextStream streamMarks(&fileMarks);
    // Stream the headers, borders, and cursors
    streamBars  << hdrBars;
    streamMarks << hdrMarks;
    streamBars  << border;
    streamMarks << border;
    streamBars  << cursorBars;
    streamMarks << cursorMarks;
    // Display floating point numbers with 4 digits of precision
    streamBars.setRealNumberPrecision(4);
    streamBars.setRealNumberNotation(QTextStream::FixedNotation);
    streamMarks.setRealNumberPrecision(4);
    streamMarks.setRealNumberNotation(QTextStream::FixedNotation);

    // Collect the ruler elements
    QMap<QString, const Element*> mapRulersSVG; // Not multimap = rehearsal marks must be spaced > 1 measure apart; not a harsh limit.
    QString cue_id;
    foreach (Page* page, score->pages()) {
        QList<const Element*> pel = page->elements();
        qStableSort(pel.begin(), pel.end(), elementLessThan);

        foreach (const Element* e, pel) {
            cue_id = getScrollCueID(score, e);
            if (!cue_id.isEmpty())
                mapRulersSVG[cue_id] = e;
        }
    }
    // For cue_id formatting: 1234567_0000007
    int startTick;
    // Stream the line and text elements, with all their attributes:
    //   cue_id, x, x1, x2, y, y1, y2, anchor, and label
    // y and y1 are fixed. That leaves:
    // cue_id, x, x1, x2, y2, anchor and label
    for (QMap<QString, const Element*>::iterator i  = mapRulersSVG.begin();
                                                 i != mapRulersSVG.end();
                                                 i++) {
        // Default Values
        // x = x1 = x2, they're all the same: a vertical line or centered text.
        // The exception is x for rehearsal mark text, which is offset right.
        cue_id = i.key();
        startTick = cue_id.left(CUE_ID_FIELD_WIDTH).toInt();
        pxX = margin + (tempos->tick2time(startTick) * pxPerSec);
        // y and label are more complex
        iY1    = 1;
        iY2    = 9;
        offX   = 0;
        label  = "";
        anchor = "";
        classVal = classRul;

        // Non-Default Values
        const Element* e = i.value();
        Element::Type eType = e->type();
        switch (eType) {
        case Element::Type::BAR_LINE :
            iBarNo = static_cast<Measure*>(e->parent()->parent())->no() + 1;
            if (iBarNo % 5 == 0) {
                // Multiples of 5 get a longer, thicker line
                iY2 = 13;
                classVal = classRul5;
                if (iBarNo % 10 == 0) {
                    // Multiples of 10 get text and a shorter, thick line
                    iY2 = 4;
                    label = QString("%1").arg(iBarNo);
                }
            }
            y    = yBars;
            break;
        case Element::Type::REHEARSAL_MARK :
            offX =  6;
            y    = yMarks;
            iY1  =  7;
            iY2  = 19;
            label  = static_cast<const Text*>(e)->xmlText();
            classVal = classMrkr;
            break;

        default:
            break;
        }
        // Stream the line element
        QTextStream* streamX = eType == Element::Type::BAR_LINE
                             ? &streamBars
                             : &streamMarks;

        *streamX << openLine  << data_cue  << cue_id    << quote
                 << cssClass  << classVal
                 << x1 << pxX << quote     << y1 << iY1 << quote
                 << x2 << pxX << quote     << y2 << iY2 << quote
                 << stroke    << closeLine << endl;

        // Only stream the text element if there's text inside it
        if (!label.isEmpty()) {
            *streamX << openText << data_cue  << cue_id << quote
                     << cssClass << classVal  << x      << pxX + offX
                     << quote    << y         << anchor << closer
                     << label    << closeText << endl;
        }
    } //for (i)
    // The bar lines ruler has a final, extra line and maybe text
    pxX = wRuler - margin;
    iY1 = 1;
    iY2 = 9;
    label = "";
    iBarNo++;
    classVal = classRul;
    if (iBarNo % 5 == 0) {
        iY2 = 13;
        classVal = classRul5;
        if (iBarNo % 10 == 0) {
            iY2 = 4;
            label = QString("%1").arg(iBarNo);
        }
    } // repeated line of code: (see *streamX << openLine above)
    streamBars << openLine  << data_cue  << cue_id    << quote
               << cssClass  << classVal
               << x1 << pxX << quote     << y1 << iY1 << quote
               << x2 << pxX << quote     << y2 << iY2 << quote
               << stroke    << closeLine << endl;
    if (!label.isEmpty()) {
        anchor = anchorEnd;
         // repeated line of code: (see *streamX << openText above)
        streamBars << openText << data_cue  << cue_id << quote
                   << cssClass << classVal  << x      << wRuler - 2
                   << quote    << y         << anchor << closer
                   << label    << closeText << endl;
    }
    // Stream the "footer", terminating the <svg> element
    streamBars  << endSVG;
    streamMarks << endSVG;
    // Flush streams, close files, and return
    streamBars.flush();
    streamMarks.flush();
    fileBars.close();
    fileMarks.close();
    return true;
}

// MuseScore::saveSMAWS() - one SVG file: with Cue IDs in data-cue attribute
//                          one VTT file: linked to the score SVG file and
//                          also linked to Bars and Markers (rulers) SVG files.
// ¡¡¡Warning!!!
// With the page settings option for points/pixels, rounding is not an issue.
// But you must choose that option, otherwise rounding errors persist due
// to the default templates being in mm (or inches, either way).
//
bool MuseScore::saveSMAWS(Score* score, const QString& saveName)
{
    // saveName is a VTT file, this needs an SVG file
    const QString fileRoot = saveName.left(saveName.size() - 4);
    const QString fileSVG  = QString("%1%2").arg(fileRoot).arg(EXT_SVG);
    // Initialize MuseScore SVG Export variables
    SvgGenerator printer;
    QPainter p;
    if (!svgInit(score, fileSVG, &printer, &p, true))
        return false;
    // Custom SMAWS header, including proper reference to MuseScore
    const QString SMAWS_VERSION = "2.1";
    printer.setDescription(QString(
            "&#x00A9;%1 ~ sidewayss.com, generated by MuseScore %2 + SMAWS %3")
            .arg(QDate::currentDate().year()).arg(VERSION).arg(SMAWS_VERSION));

    // Start & End | Ticks & Time:
    // VTT needs start & end times, and I need ticks to calculate time.
    // cue_id is what links the SVG elements to the VTT cues.
    // It is a unique id because it is in this format:
    //     "startTick_endTick"
    // and startTick + endTick (duration) is unique to each cue, which refers
    // to an SVG ChordRest, BarLine, or RehearsalMark, across staves/voices.
    int     startTick, endTick;
    QString cue_id;
    // setVTT - a chronologically sorted set of unique cue_ids
    QStringList setVTT; // QSet is unordered, I need sorting.
    // mapSVG - a real map: key = cue_id, value = list of elements
    QMultiMap<QString, Element*>  mapSVG;

    // Animated elements in a multi-page file? It's unnecessary IMO.
    // + saveSvg() handles pages in SVG with a simple horizontal offset.
    // + SVG doesn't even have support for pages, per se (yet?).
    // So this code only exports the score's first page. At least for now...
    // also note: MuseScore plans to export each page as a separate file.
    Page* page = score->pages()[0];

    printer.setCueID("");
    bool isScrollVertical = score->pageFormat()->twosided();
    printer.setScrollAxis(isScrollVertical); // true = y axis, a hack for now.

    // 0th pass MuseScore SVG: an entire pass just for staff lines
    paintStaffLines(score, page, &p, &printer);

    // 1st pass MuseScore SVG: the rest of the non-animated elements
    QList<const Element*> pel = page->elements();
    qStableSort(pel.begin(), pel.end(), elementLessThan);

    Element::Type eType;        // everything is determined by element type
    qreal         xoffClef = 0; // for frozen pane formatting
    foreach (const Element* e, pel) {
        eType = e->type();
        // Always exclude invisible elements ...unless they're tempo changes.
        if (!e->visible() && eType != Element::Type::TEMPO_TEXT)
                continue;
        // Exclude animated elements from this 1st pass, except those used
        // for scrolling cues: BarLine, RehearsalMark, Clef, KeySig, TimeSig.
        switch (eType) {
        case Element::Type::STAFF_LINES : // Not animated, handled above.
        case Element::Type::NOTE        : // Notes,
        case Element::Type::REST        : // ...rests,
        case Element::Type::NOTEDOT     : // ...and
        case Element::Type::ACCIDENTAL  : // ...optional
        case Element::Type::LYRICS      : // ...accessories.
        case Element::Type::HARMONY     : // Chord text/symbols.
            continue; // Exclude from 1st pass
            break;
        case Element::Type::BAR_LINE       :
        case Element::Type::REHEARSAL_MARK :
            // Add the cue ID to the VTT set.
            cue_id = getScrollCueID(score, e);
            if (!cue_id.isEmpty()) {
                setVTT.append(cue_id);
                // RehearsalMarks only animate in the Ruler, not in the score.
                // Same VTT file, different SVG file. See saveSMAWS_Rulers().
                if (eType == Element::Type::REHEARSAL_MARK)
                    cue_id = "";
            }
            break;
        case Element::Type::CLEF       : // clefs, keysigs, timesigs, tempos...
        case Element::Type::KEYSIG     : // ...are in a frozen pane when
        case Element::Type::TIMESIG    : // ...scrolling horizontally.
        case Element::Type::TEMPO_TEXT : // We must build that pane here:
            // Create the cue_id
            cue_id = getScrollCueID(score, e);
            // Add it to setVTT
            setVTT.append(cue_id);

            // Frozen Pane:
            //
            // 1st Clef provides initial x-offset
            // After that each element uses a fixed width I provide.
            // KeySigs vary constantly by the number of accidentals.
            // TimeSigs are are a constant width.
            // Clef is the one variable-width element type in the frozen pane,
            // but that variation is only 1pt, so I use my widest clef: Alto.
            // Note: This means that the code does not support ClefType::G3_O,
            // which is the widest clef available in MuseScore.
            if (xoffClef == 0 && eType == Element::Type::CLEF)
                xoffClef = e->x();
            // All these elements generate a <g> in <defs>

            // Each element type has a <use> element for display

            break;
        default:
            cue_id = ""; // Default is un-animated (inanimate?) element
            break;
        }
        // Set the Element pointer inside SvgGenerator/SvgPaintEngine
        printer.setElement(e);
        // Set the cue_id, even if it's empty (especially if it's empty)
        printer.setCueID(cue_id);
        // Paint it
        paintElement(p, e);
    }
    // 2nd pass: Deal with elements excluded in 1st pass.
    // Animated elements must be sorted in playback order.
    // So we iterate:
    //     by Measure, by Segment, by Track, by SegmentType, by Element
    // And we collect:
    //     into setVTT and mapSVG.
    // After it's all collected, we write the files.
    //
    // Iterate and collect:
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasureMM()) {
        for (Segment* s = m->first(); s; s = s->next()) {
            for (int t = 0, n = score->ntracks(); t < n; t++) {
                Segment::Type sType = s->segmentType();
                switch (sType) {
                // ChordRests are the highlighted elements in the animation
                case Segment::Type::ChordRest : {
                    // Not all tracks within the segment will have content
                    if (s->cr(t) == 0)
                        continue;
                    // We have a ChordRest, now we operate
                    ChordRest* cr = s->cr(t);
                    // Create the cue_id
                    startTick = cr->tick();
                      endTick = startTick + cr->actualTicks();
                       cue_id = getCueID(startTick, endTick);
                    // Add it to setVTT
                    setVTT.append(cue_id);

                    // The rest of the loop is spent populating mapSVG
                    // Is it a chord or a rest?
                    switch (cr->type()) {
                    case Element::Type::REST  :
                        // Insert it
                        mapSVG.insert(cue_id, cr);
                        break;
                    case Element::Type::CHORD : {
                        // Chords have notes, and notes have accidentals + dots.
                        // For each note: add note + accidental + dots to mapSVG.
                        // Both notes and rests have dots in the score, but in
                        // MuseScore C++ only notes have dots. MAX_DOTS = 3;
                        QList<Note*> notes = static_cast<Chord*>(cr)->notes();
                        for (int i = 0, z = notes.size(); i < z; i++) {
                            // Note (note head)
                            mapSVG.insert(cue_id, notes[i]);
                            // Accidental
                            if (notes[i]->accidental() != 0)
                                mapSVG.insert(cue_id, notes[i]->accidental());
                            // Dots
                            for (int j = 0; j < MAX_DOTS; j++) {
                                if (notes[i]->dot(j) != 0)
                                    mapSVG.insert(cue_id, notes[i]->dot(j));
                            }
                        }
                        break; }
                    default:
                        break;
                    }
                    // Insert lyrics
                    for (int l = 0; l < cr->lyricsList().size(); ++l) {
                        mapSVG.insert(cue_id, cr->lyrics(l));
                    }
                    // Chord symbols (Type::HARMONY) inherit duration from
                    // the note they are attached to. I need them to last until
                    // the next chord change = new cue_id = must be last step.
                    for (Element* eAnn : s->annotations()) {
                        if (eAnn->type() == Element::Type::HARMONY) {
                            cue_id = getAnnCueID(score, s, eAnn->type());
                            setVTT.append(cue_id);
                            mapSVG.insert(cue_id, eAnn);
                        }
                    }
                    break; } // Segment::Type::ChordRest
                default:
                    break;
                } // switch (segment type)
            } // for (tracks)
        } // for (segments)
    } // for (measures)
    //
    // Write the VTT file:
    //
    if (!saveVTT(score, fileRoot, setVTT))
        return false;
    //
    // Write the SVG file: 2nd pass, animated elements
    // mapSVG is already sorted, no need to loop by key.
    //
    for (QMultiMap<QString, Element*>::iterator i = mapSVG.begin();
                                                i != mapSVG.end();
                                                i++) {
        Element* e = i.value();
        if(e->visible()) {
            printer.setElement(e);
            printer.setCueID(i.key());
            paintElement(p, e);
        }
    }
    // Clean up and return
    score->setPrinting(false);
    MScore::pdfPrinting = false;
    p.end(); // Writes MuseScore SVG file to disk, finally
    return true;
}
// End SVG / SMAWS
///////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------
//   createThumbnail
//---------------------------------------------------------

static QPixmap createThumbnail(const QString& name)
      {
      Score* score = new Score;
      Score::FileError error = readScore(score, name, true);
      if (error != Score::FileError::FILE_NO_ERROR)
            return QPixmap();
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
