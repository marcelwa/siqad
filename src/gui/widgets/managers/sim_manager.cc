// @file:     sim_manager.cc
// @author:   Samuel
// @created:  2017.09.27
// @editted:  2017.09.27 - Samuel
// @license:  GNU LGPL v3
//
// @desc:     window that allows users to setup and dispatch new simulations,
//            as well as manage ongoing or completed simulations.

#include "sim_manager.h"
#include "../../../global.h"


namespace gui{

QString python_path;

// Qt::Dialog makes the main window unclickable. Use Qt::Window if this behavior should be changed.
SimManager::SimManager(QWidget *parent)
  : QWidget(parent, Qt::Dialog)
{
  if (gui::python_path.isEmpty())
    initPythonPath();
  initEngines();
  initSimManager();
  initSimSetupDialog();
}

SimManager::~SimManager()
{
  for(prim::SimEngine *engine : sim_engines)
    delete engine;
  sim_engines.clear();

  for(prim::SimJob *job : sim_jobs)
    delete job;
  sim_jobs.clear();
}


void SimManager::showSimSetupDialog()
{
  sim_setup_dialog->show();
  le_job_nm->setText(defaultJobName());
}

bool SimManager::addJob(prim::SimJob *job)
{
  if(!job)
    return false;
  sim_jobs.prepend(job);
  return true;
}


prim::SimEngine *SimManager::getEngine(const QString &name)
{
  for (prim::SimEngine *engine : sim_engines) {
    if (engine->name() == name)
      return engine;
  }
  // not found
  return 0;
}



// PRIVATE

void SimManager::initPythonPath()
{
  QString s_py = settings::AppSettings::instance()->get<QString>("user_python_path");

  if (!s_py.isEmpty()) {
    gui::python_path = s_py;
    qDebug() << tr("Python path retrieved from user settings: %1").arg(gui::python_path);
  } else {
    if (!findWorkingPythonPath())
      qWarning() << "No Python 3 interpreter found. Please set it in the settings dialog.";
  }
}

bool SimManager::findWorkingPythonPath()
{
  QStringList test_py_paths;
  QString kernel_type = QSysInfo::kernelType();
  auto get_py_paths = [](const QString &os) -> QStringList {
    return settings::AppSettings::instance()->getPaths("python_search_"+os);
  };
  if (kernel_type == "linux" || kernel_type == "freebsd") {
    test_py_paths << get_py_paths("linux");
  } else if (kernel_type == "winnt") {
    test_py_paths << get_py_paths("winnt");
  } else if (kernel_type == "darwin") {
    test_py_paths << get_py_paths("darwin");
  } else {
    qWarning() << tr("No Python search path defined for your kernel type %1. Please enter your Python binary path in the Settings dialog and restart the application.").arg(kernel_type);
    return false;
  }

  QString test_script = QDir(QCoreApplication::applicationDirPath()).filePath("src/phys/is_python3.py");
  if (!QFile::exists(test_script)) {
    qDebug() << tr("Python version test script %1 not found").arg(test_script);
    return false;
  }


  for (QString test_py_path : test_py_paths) {
    QStringList splitted_path = test_py_path.split(',');
    if (splitted_path.size() == 0)
      continue;

    // set up command and arguments
    QString command = splitted_path.at(0);
    QStringList args = splitted_path.mid(1);
    args << test_script;

    QString output;
    QProcess *py_process = new QProcess(this);
    //py_process->start(test_py_path, {test_script});
    py_process->start(command, args);
    py_process->waitForStarted(1000);

    // run the test script
    while(py_process->waitForReadyRead(1000))
      output.append(QString::fromStdString(py_process->readAll().toStdString()));
    
    if (output.contains("Python3 Interpretor Found")) {
      gui::python_path = test_py_path;
      qDebug() << tr("Python path found: %1").arg(gui::python_path);
      return true;
    } else {
      qDebug() << tr("Python path %1 is invalid. Output: %2").arg(test_py_path).arg(output);
    }
  }

  return false;
}

void SimManager::initSimManager()
{
  // init simulation manager GUI

  // simulator manager panes
  sim_list_pan = new QListWidget();
  sim_actions_pan = new QVBoxLayout();

  // simulator manager layout
  QHBoxLayout *man_main = new QHBoxLayout();
  man_main->addWidget(sim_list_pan);
  man_main->addLayout(sim_actions_pan);

  setLayout(man_main);

  setWindowTitle(tr("Simulation Manager"));
}

void SimManager::initSimSetupDialog()
{
  sim_setup_dialog = new QWidget(this, Qt::Dialog);

  // Engine Select Group, can be written here since it's only initialized once.
  QGroupBox *engine_sel_group = new QGroupBox(tr("Engine Selection"));

  QLabel *label_eng_sel = new QLabel(tr("Engine:"));
  cb_eng_sel = new QComboBox();
  cb_eng_sel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  updateEngineSelectionList();

  QLabel *label_job_nm = new QLabel(tr("Job Name:"));
  le_job_nm = new QLineEdit(defaultJobName());

  QFormLayout *engine_sel_fl = new QFormLayout;
  engine_sel_fl->addRow(label_eng_sel, cb_eng_sel);
  engine_sel_fl->addRow(label_job_nm, le_job_nm);

  engine_sel_group->setLayout(engine_sel_fl);

  // updates parameter list based on selection
  connect(cb_eng_sel, QOverload<int>::of(&QComboBox::currentIndexChanged), 
          [this](){updateSimParams();});

  // Sim Params Group, this will change depending on combo box selection.
  sim_params_group = new QGroupBox(tr("Simulation Parameters"));
  sim_params_vl = new QVBoxLayout;
  updateSimParams();
  sim_params_group->setLayout(sim_params_vl);


  // Buttons
  button_run = new QPushButton(tr("&Run"));
  button_run->setShortcut(Qt::Key_Return);
  button_cancel = new QPushButton(tr("Cancel"));
  button_cancel->setShortcut(Qt::Key_Escape);

  QToolButton *tb_more = new QToolButton();
  tb_more->setPopupMode(QToolButton::InstantPopup);

  connect(button_run, &QAbstractButton::clicked,
          this, &gui::SimManager::submitSimSetup);
  connect(button_cancel, &QAbstractButton::clicked,
          sim_setup_dialog, &QWidget::hide);

  // save or reset settings
  QMenu *menu_more = new QMenu("More");
  QAction *action_save_as_default = new QAction("Save as Default");
  QAction *action_reset_to_usr_default = new QAction("Reset to User Default");
  QAction *action_reset_to_eng_default = new QAction("Reset to Engine Default (also deletes user default)");
  tb_more->setText("Save/Reset");
  tb_more->setMenu(menu_more);
  menu_more->addAction(action_save_as_default);
  menu_more->addAction(action_reset_to_usr_default);
  menu_more->addAction(action_reset_to_eng_default);

  connect(action_save_as_default, &QAction::triggered,
          this, &gui::SimManager::saveSettingsAsDefault);
  connect(action_reset_to_usr_default, &QAction::triggered,
          this, &gui::SimManager::resetToUserDefault);
  connect(action_reset_to_eng_default, &QAction::triggered,
          this, &gui::SimManager::resetToEngineDefault);

  // layouts
  bottom_buttons_hl = new QHBoxLayout;
  bottom_buttons_hl->addStretch(1);
  bottom_buttons_hl->addWidget(button_run);
  bottom_buttons_hl->addWidget(button_cancel);
  bottom_buttons_hl->addWidget(tb_more);

  // Combine into one dialog
  new_setup_dialog_l = new QVBoxLayout;
  new_setup_dialog_l->addWidget(engine_sel_group);
  new_setup_dialog_l->addWidget(sim_params_group);
  new_setup_dialog_l->addLayout(bottom_buttons_hl);

  sim_setup_dialog->setLayout(new_setup_dialog_l);
}

//only called when cb_eng_sel selection is changed.
void SimManager::updateSimParams()
{
  // clear out existing sim params layout
  QLayoutItem *child;
  while ((child = sim_params_vl->takeAt(0)) != 0) {
    child->widget()->hide();
    delete child->widget();
    delete child;
  }

  // add the property form of the currently selected engine
  prim::SimEngine *curr_engine = getEngine(cb_eng_sel->currentText());
  PropertyMap sim_params_map = curr_engine->sim_params_map;
  if (!sim_params_map.isEmpty()) {
    // update the map with user configurations
    QString usr_cfg_file_path = curr_engine->userConfigurationFilePath();
    if (QFileInfo::exists(usr_cfg_file_path))
      sim_params_map.updateValuesFromXML(usr_cfg_file_path);

    // create a property form with the map and show
    curr_sim_params_form = new PropertyForm(sim_params_map, this);
    curr_sim_params_form->show();
    sim_params_vl->addWidget(curr_sim_params_form);
  } else {
    // no parameters to show
    curr_sim_params_form = 0;
    sim_params_vl->addWidget(new QLabel("No simulation parameters available for this engine."));
  }

  sim_setup_dialog->adjustSize();
}


void SimManager::quickRun()
{
  le_job_nm->setText(defaultJobName());
  submitSimSetup();
}

void SimManager::updateEngineSelectionList()
{
  if(!cb_eng_sel)
    return;
  cb_eng_sel->clear();

  if(sim_engines.isEmpty())
    cb_eng_sel->addItem("No Engines");
  else
    for(auto eng : sim_engines)
      cb_eng_sel->addItem(eng->name());
}


QString SimManager::defaultJobName()
{
  return "SIM_" + QDateTime::currentDateTime().toString("yyMMdd_HHmmss");
}


void SimManager::saveSimulationPreset()
{
  prim::SimEngine *curr_engine = getEngine(cb_eng_sel->currentIndex());

  if (!curr_engine || !curr_sim_params_form) {
    qCritical() << tr("Invalid engine selection or engine doesn't have parameters");
    return;
  }

  // set up the directory
  QDir config_dir(curr_engine->userPresetDirectoryPath());
  if (!config_dir.mkpath("."))
    qWarning() << tr("Unable to create user preset directory at %1").arg(config_dir.path());

  // prompt for preset name
  bool ok;
  QString preset_name = QInputDialog::getText(this, tr("Preset name"),
                                              tr("Preset name:"), QLineEdit::Normal,
                                              tr("Custom Preset"), &ok);
  if (!ok) {
    return;
  } else if (preset_name.isEmpty()) {
    QMessageBox msg;
    msg.setText("The preset name cannot be empty, aborting.");
    msg.exec();
    return;
  }
  // TODO check for preset name conflicts with existing presets

  QString preset_path = config_dir.filePath(preset_name);
  QFile f(preset_path);
  if (!f.open(QIODevice::WriteOnly)) {
    qCritical() << tr("Error when opening file to save preset.");
    return;
  }

  QXmlStreamWriter ws(&f);
  qDebug() << tr("Beginning preset writing to %1").arg(preset_path);
  ws.setAutoFormatting(true);
  ws.writeStartDocument();

  ws.writeStartElement("properties");
  PropertyMap::writeValuesToXMLStream(curr_sim_params_form->finalProperties(), &ws);
  ws.writeEndElement();

  f.close();
  qDebug() << tr("Preset writing complete");
}


void SimManager::submitSimSetup()
{
  prim::SimEngine *curr_engine = getEngine(cb_eng_sel->currentIndex());

  if (!curr_engine) {
    qCritical() << tr("Invalid engine selection");
    return;
  }

  // hide setup dialog
  sim_setup_dialog->hide();

  // create job object and extract simulation parameters from the engine's parameter widget
  prim::SimJob *new_job = new prim::SimJob(le_job_nm->text(), curr_engine);
  new_job->addSimParams(curr_sim_params_form->finalProperties());

  addJob(new_job);
  emit sig_simJob(new_job);
}


void SimManager::saveSettingsAsDefault()
{
  prim::SimEngine *curr_engine = getEngine(cb_eng_sel->currentIndex());

  if (!curr_engine || !curr_sim_params_form) {
    qCritical() << tr("Invalid engine selection or engine doesn't have parameters");
    return;
  }

  QString usr_cfg_file_path = curr_engine->userConfigurationFilePath();
  QFileInfo usr_cfg_file_inf(usr_cfg_file_path);
  usr_cfg_file_inf.dir().mkpath(".");
  QFile write_file(usr_cfg_file_path);

  if (!write_file.open(QIODevice::WriteOnly)) {
    qCritical() << tr("Export Simulation Settings: error when opening file to save");
    return;
  }
  QXmlStreamWriter ws(&write_file);
  qDebug() << tr("Beginning export to %1").arg(usr_cfg_file_path);
  ws.setAutoFormatting(true);
  ws.writeStartDocument();

  ws.writeStartElement("properties");
  PropertyMap::writeValuesToXMLStream(curr_sim_params_form->finalProperties(), &ws);
  ws.writeEndElement();

  write_file.close();
  qDebug() << tr("Export complete");
}


void SimManager::resetToUserDefault()
{
  updateSimParams();
}


void SimManager::resetToEngineDefault()
{
  prim::SimEngine *curr_engine = getEngine(cb_eng_sel->currentIndex());
  QFile usr_cfg_file(curr_engine->userConfigurationFilePath());
  if (usr_cfg_file.remove()) {
    qDebug() << tr("Removed user config file");
  } else {
    qDebug() << tr("No user config file to remove");
  }
  updateSimParams();
}


void SimManager::initEngines()
{
  //QString engine_lib_dirs = settings::AppSettings::instance()->getPath("phys/eng_lib_dirs");
  //QStringList engine_lib_dir_paths = engine_lib_dirs.split(';', QString::SkipEmptyParts);
  QStringList engine_lib_dir_paths = settings::AppSettings::instance()->getPaths("phys/eng_lib_dirs");

  for (QString engine_lib_dir_path : engine_lib_dir_paths) {
    QDir engine_lib_dir(engine_lib_dir_path);
    if (!engine_lib_dir.exists()) {
      qDebug() << tr("Engine lib path ignored: %1").arg(engine_lib_dir_path);
      continue;
    }
    qDebug() << tr("Engine lib path found: %1").arg(engine_lib_dir_path);
    QStringList engine_dir_paths = engine_lib_dir.entryList(QStringList({"*"}),
        QDir::AllDirs | QDir::NoDotAndDotDot);

    // find all existing engines in the engine library
    // TODO edit this part to store file names rather than QDirs
    QList<QDir> engine_dirs;
    QStringList engine_declaration_files;
    QStringList engine_filter(QStringList() << "*.physeng" << "engine_description.xml");
    for (QString engine_dir_path : engine_dir_paths) {
      qDebug() << tr("SimManager: Checking %1 for engine description file").arg(engine_dir_path);
      QDir eng_dir(engine_lib_dir.filePath(engine_dir_path));
      QStringList matched_eng_files = eng_dir.entryList(engine_filter, QDir::Files);

      //qDebug() << tr("Found %1 engine files").arg(matched_eng_files.length());

      // add engine declaration files to a list
      for (QString matched_eng_file : matched_eng_files) {
        engine_declaration_files << eng_dir.absoluteFilePath(matched_eng_file);
        qDebug() << tr("Found engine file: %1").arg(engine_declaration_files.back());
      }
    }

    // import engines found above
    for (QString eng_dec_file : engine_declaration_files) {
      // read each engine description file and add to list
      sim_engines.append(new prim::SimEngine(eng_dec_file));
    }
  }

  qDebug() << tr("Successfully read physics engine files");
}

} // end gui namespace
