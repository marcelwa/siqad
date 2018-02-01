// @file:     sim_job.h
// @author:   Samuel
// @created:  2017.10.10
// @editted:  2017.10.10 - Samuel
// @license:  GNU LGPL v3
//
// @desc:     SimJob object that describes a simulation job and stores the results from that job

#ifndef _PRIM_SIM_JOB_H_
#define _PRIM_SIM_JOB_H_

#include <QtWidgets>
#include <QtCore>
#include "sim_engine.h"
#include "src/settings/settings.h" // TODO probably need this later

namespace prim{

  class SimJob : public QObject
  {
    Q_OBJECT
  public:
    // constructor
    SimJob(const QString &nm, SimEngine *eng=0, QWidget *parent=0);

    // destructor
    ~SimJob() {};

    // JOB SETUP

    // TODO emit signal from SimJob to save job, instead of letting design panel call
    // invoke binary.
    // bool saveJob(const QString &);


    // load job from XML (for jobs that keep running even if parent terminates)
    // TODO sim_manager probably needs to check folders for unfinished simulations
    bool loadJob(const QString &) {return false;}

    // simulation parameters
    QList<QPair<QString, QString>> simParams() const {return sim_params;}
    void addSimParam(const QString &field, const QString &value) {sim_params.append(qMakePair(field, value));}
    void addSimParams(const QList<QPair<QString, QString>> &add_params) {sim_params.append(add_params);}
    QList<QPair<QString, QString>> loadSimParamsFromDialog();
    void loadSimParamsFromEngineDialog();


    // JOB EXECUTION

    // call sim engine binary
    bool invokeBinary();


    // JOB RESULT

    bool readResults();     // read result XML
    bool processResults();  // process results

    // result storage and access
    // elec_dists struct for showing where electrons are
    struct elecDist{
      QString dist_str;
      QList<int> dist_ls; // TODO might not be necessary
      float energy;
    };

    int distCount() {return dist_count;}  // return the number of charge distributions this has

    // ACCESSORS

    // job name
    QString name() {return job_name;}

    // engine used
    void setEngine(SimEngine *eng) {engine = eng;}
    //prim::SimEngine *engine() {return engine;}
    QString engineName() {return engine ? engine->name() : "Undefined";}

    QString runtimeTempDir();     // runtime job directory
    QString problemFile();        // runtime problem file
    QString resultFile();         // runtime result file

    QDateTime startTime() {return start_time;}
    QDateTime endTime() {return end_time;}

    bool isComplete() {return completed;} // indicate whether the job has been completed

    QString terminalOutput() {return terminal_output;}
    void saveTerminalOutput();
    
    // variables TODO put them back to private later, with proper accessors
    QList<QPair<float,float>> physlocs;   // physlocs[dot_ind].first or .second
    QList<QList<int>> elec_dists;         // elec_dists[result_ind][dot_ind] TODO change this to QList of QVectors
  private:

    void deduplicateDist();           // deduplicate charge distribution results
    
    // variables
    QString job_name;         // job name for identification
    SimEngine *engine;        // the engine used by this job
    QString run_job_dir;      // job directory for storing runtime data
    QString problem_path;     // path to problem file
    QString result_path;      // path to result file
    QString terminal_output;  // terminal output from the job
    QDateTime start_time, end_time; // start and end times of the job
    QProcess *sim_process;    // runtime process of the job
    QStringList cml_arguments;// command line arguments when invoking the job
    bool completed;           // whether the job has completed simulation

    // read xml
    QStringList ignored_xml_elements; // XML elements to ignore when reading results

    // parameters
    QList<QPair<QString, QString>> sim_params;

    // results
    // TODO flag storing what types of results are available
    int dist_count; // number of distributions
  };

} // end of prim namespace

#endif
