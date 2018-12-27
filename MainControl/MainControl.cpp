#include "MainControl.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDesktopServices>
#include <QSettings>
#include <QTextCodec>
#include <QTextStream>
#include <iostream>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QFile>
#include <QAxObject>
#include <QAxWidget>
#include <windows.h>
#include "CommonFuns.h"
#include "ServerFunc.h"
#include "zip.h"

MainControl::MainControl(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	setWindowIcon(QIcon(QStringLiteral(":/HCity.ico")));
	LoadSetting();

	cloud_process_ = new QProcess(this);
	connect(cloud_process_, &QProcess::readyReadStandardOutput, this, &MainControl::CloudReadyReadStandardOutput);
	connect(cloud_process_, &QProcess::readyReadStandardError, this, &MainControl::CloudReadyReadStandardOutput);
 	connect(cloud_process_, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(CloudFinished(int, QProcess::ExitStatus)));
 
 	brid_process_ = new QProcess(this);
 	connect(brid_process_, &QProcess::readyReadStandardOutput, this, &MainControl::BridReadyReadStandardOutput);
 	connect(brid_process_, &QProcess::readyReadStandardError, this, &MainControl::BridReadyReadStandardOutput);
 	connect(brid_process_, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(BridFinished(int, QProcess::ExitStatus)));
}


MainControl::~MainControl()
{
}

void MainControl::SaveSetting()
{
 	QString filename = QApplication::applicationDirPath() + QStringLiteral("/GuiConfig.json");
	boost::property_tree::ptree pt;
	pt.put(("线路名称"), ui.lineEdit_Circuit_Name->text().toLocal8Bit().data());
	pt.put(("航行路径"), ui.lineEdit_FlightInfomation->text().toLocal8Bit().data());
	pt.put(("分段区间"), ui.lineEdit_Circuit_Range->text().toLocal8Bit().data());
	pt.put(("电压等级"), ui.lineEdit_Kv->text().toLocal8Bit().data());
	pt.put(("采集日期"), ui.lineEdit_Date->text().toLocal8Bit().data());
	pt.put(("点云.点云路径"), ui.lineEdit_Cloud_CloudPath->text().toLocal8Bit().data());
	pt.put(("点云.铁塔数据目录"), ui.lineEdit_Cloud_TowerDir->text().toLocal8Bit().data());
	pt.put(("点云.结果目录"), ui.label_Cloud_ResultDir->text().toLocal8Bit().data());
	pt.put(("点云.样本目录"), ui.lineEdit_Cloud_ClassDir->text().toLocal8Bit().data());

	std::ofstream ofs(filename.toLocal8Bit().data(), std::fstream::out);
	boost::property_tree::write_json(ofs, pt);
	ofs.close();


	// 转成utf8
	std::string json;
	std::ifstream ifs(filename.toLocal8Bit().data(), std::ifstream::in);
	if (ifs.is_open())
	{
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		json = buffer.str();
		ifs.close();
	}


	ofs.clear();
	ofs.open(filename.toLocal8Bit().data(), std::ios::out | std::ios::binary);
	if (ofs.is_open())
	{
		json = ChartSetConv::C2W(json);
		ofs << json;
		ofs.close();
	}
}

void MainControl::LoadSetting()
{
	QString filename = QApplication::applicationDirPath() + QStringLiteral("/GuiConfig.json");
	if (!QFileInfo::exists(filename))
		return;

 	std::ifstream is(filename.toLocal8Bit().data(), std::ios::in);

	boost::property_tree::ptree pt;                       //define property_tree object
 	if (is.is_open())
 	{
 		try {
 			read_json(is, pt);          //parse json
 			is.close();
 		}
 		catch (...) {
 			return;
 		}
 
 		is.close();
 	}
	
	try {
		ui.lineEdit_Circuit_Name->setText(QString::fromUtf8(pt.get_optional<std::string>(ChartSetConv::C2W("线路名称")).value().c_str()));
		ui.lineEdit_FlightInfomation->setText(QString::fromUtf8(pt.get_optional<std::string>(ChartSetConv::C2W("航行路径")).value().c_str()));
		ui.lineEdit_Circuit_Range->setText(QString::fromUtf8(pt.get_optional<std::string>(ChartSetConv::C2W("分段区间")).value().c_str()));
		ui.lineEdit_Kv->setText(QString::fromUtf8(pt.get_optional<std::string>(ChartSetConv::C2W("电压等级")).value().c_str()));
		ui.lineEdit_Date->setText(QString::fromUtf8(pt.get_optional<std::string>(ChartSetConv::C2W("采集日期")).value().c_str()));

		boost::property_tree::ptree dianyun = pt.get_child(ChartSetConv::C2W("点云"));
		ui.lineEdit_Cloud_CloudPath->setText(QString::fromUtf8(dianyun.get_optional<std::string>(ChartSetConv::C2W("点云路径")).value().c_str()));
		ui.lineEdit_Cloud_TowerDir->setText(QString::fromUtf8(dianyun.get_optional<std::string>(ChartSetConv::C2W("铁塔数据目录")).value().c_str()));
		ui.label_Cloud_ResultDir->setText(QString::fromUtf8(dianyun.get_optional<std::string>(ChartSetConv::C2W("结果目录")).value().c_str()));
		ui.lineEdit_Cloud_ClassDir->setText(QString::fromUtf8(dianyun.get_optional<std::string>(ChartSetConv::C2W("样本目录")).value().c_str()));
	}
	catch (...) {
		return;
	}
}

std::map<QString, std::map<QString, QString>> MainControl::LoadImageDescription()
{
	std::map<QString, std::map<QString, QString>> res;
	QString filepath = ui.lineEdit_FlightInfomation->text();
	if (!QFile(filepath).exists())
	{
		std::cout << "void LoadTowers()  !QFile(filepath).exists()" << std::endl;
		return res;
	}

	HRESULT r = OleInitialize(0);
	if (r != S_OK && r != S_FALSE) {
		std::cout << "Qt: Could not initialize OLE(error" << r << ")" << std::endl;
		return res;
	}
	std::cout << "LoadTowers filepath" << filepath.toLocal8Bit().data() << std::endl;
	QAxObject excel("Excel.Application");
	excel.setProperty("DisplayAlerts", false);//不显示任何警告信息
	excel.setProperty("Visible", false); //隐藏打开的excel文件界面
	QAxObject *workbooks = excel.querySubObject("WorkBooks");
	QAxObject *workbook = workbooks->querySubObject("Open(QString, QVariant)", filepath); //打开文件
	QAxObject * worksheet = workbook->querySubObject("WorkSheets(int)", 1); //访问第一个工作表
	QAxObject * usedrange = worksheet->querySubObject("UsedRange");
	QAxObject * rows = usedrange->querySubObject("Rows");
	int intRows = rows->property("Count").toInt() * 2; //行数

	QString Range = "A1:H" + QString::number(intRows);
	QAxObject *allEnvData = worksheet->querySubObject("Range(QString)", Range); //读取范围
	QVariant allEnvDataQVariant = allEnvData->property("Value");
	QVariantList allEnvDataList = allEnvDataQVariant.toList();


	QStringList heads = allEnvDataList[0].toStringList();

	for (int i = 2; i < intRows - 2; i+=2)
	{
		std::map<QString, QString> point_info;
		QStringList allEnvDataList_i = allEnvDataList[i].toStringList();
		for (int j = 0; j < allEnvDataList_i.size(); ++j)
		{
			point_info[heads[j]] = allEnvDataList_i[j];
			//ui.textEdit_CloudLog->insertPlainText(heads[j] + QStringLiteral("：") + allEnvDataList_i[j] + QStringLiteral("\t"));
		}
		//ui.textEdit_CloudLog->insertPlainText(QStringLiteral("\n"));
		if (!point_info[QStringLiteral("经度")].isEmpty() && !point_info[QStringLiteral("照片名称")].isEmpty())
		{
			point_info[QStringLiteral("经度")] = point_info[QStringLiteral("经度")].replace('E', "").replace('W', '-');
			point_info[QStringLiteral("纬度")] = point_info[QStringLiteral("纬度")].replace('N', "").replace('S', '-');
			res[point_info[QStringLiteral("照片名称")]] = point_info;
		}
		
	}

	workbook->dynamicCall("Close (Boolean)", false);
	excel.dynamicCall("Quit()");
	OleUninitialize();

	return res;
}

void MainControl::WriteFlightPath(QString filename)
{
	boost::property_tree::ptree pt;
	
	std::map<QString, QString> flight_info = LoadFlightInfomation();
	if (!flight_info.size())
		return;

	boost::property_tree::ptree errpt_array;
	for (std::map<QString, QString>::iterator it = flight_info.begin(); it != flight_info.end(); ++it)
	{
		boost::property_tree::ptree point_pt;
		point_pt.put("position", it->second.toLocal8Bit().data());
		point_pt.put("time", it->first.toLocal8Bit().data());

		errpt_array.push_back(std::make_pair("", point_pt));
	}
	pt.put_child("roamPoints", errpt_array);

	std::ofstream ofs(filename.toLocal8Bit().data(), std::fstream::out);
	boost::property_tree::write_json(ofs, pt);
	ofs.close();
	// 转成utf8
	std::string json;
	std::ifstream ifs(filename.toLocal8Bit().data(), std::ifstream::in);
	if (ifs.is_open())
	{
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		json = buffer.str();
		ifs.close();
	}


	ofs.clear();
	ofs.open(filename.toLocal8Bit().data(), std::ios::out | std::ios::binary);
	if (ofs.is_open())
	{
		json = ChartSetConv::C2W(json);
		ofs << json;
		ofs.close();
	}
}

std::map<QString, QString> MainControl::LoadFlightInfomation()
{
	std::map<QString, QString> res;
	QString filepath = ui.lineEdit_FlightInfomation->text();
	if (!QFile(filepath).exists())
	{
		std::cout << "void LoadTowers()  !QFile(filepath).exists()" << std::endl;
		return res;
	}

	HRESULT r = OleInitialize(0);
	if (r != S_OK && r != S_FALSE) {
		std::cout << "Qt: Could not initialize OLE(error" << r << ")" << std::endl;
		return res;
	}
	std::cout << "LoadTowers filepath" << filepath.toLocal8Bit().data() << std::endl;
	QAxObject excel("Excel.Application");
	excel.setProperty("DisplayAlerts", false);//不显示任何警告信息
	excel.setProperty("Visible", false); //隐藏打开的excel文件界面
	QAxObject *workbooks = excel.querySubObject("WorkBooks");
	QAxObject *workbook = workbooks->querySubObject("Open(QString, QVariant)", filepath); //打开文件
	QAxObject * worksheet = workbook->querySubObject("WorkSheets(int)", 1); //访问第一个工作表
	QAxObject * usedrange = worksheet->querySubObject("UsedRange");
	QAxObject * rows = usedrange->querySubObject("Rows");
	int intRows = rows->property("Count").toInt() * 2; //行数

	QString Range = "A1:H" + QString::number(intRows);
	QAxObject *allEnvData = worksheet->querySubObject("Range(QString)", Range); //读取范围
	QVariant allEnvDataQVariant = allEnvData->property("Value");
	QVariantList allEnvDataList = allEnvDataQVariant.toList();


	QStringList heads = allEnvDataList[0].toStringList();
	int log_index = heads.indexOf(QStringLiteral("经度"));
	int lat_index = heads.indexOf(QStringLiteral("纬度"));
	int z_index = heads.indexOf(QStringLiteral("高度"));
	int time_index = heads.indexOf(QStringLiteral("时间戳"));


	for (int i = 2; i < intRows - 2; i += 2)
	{
		QStringList allEnvDataList_i = allEnvDataList[i].toStringList();
		if (!allEnvDataList_i[time_index].isEmpty() && !allEnvDataList_i[log_index].isEmpty())
		{
			res[allEnvDataList_i[time_index]] = allEnvDataList_i[log_index].replace("E", "").replace('W', '-') + QStringLiteral(",") + allEnvDataList_i[lat_index].replace("N", "").replace('S', '-') + QStringLiteral(",") + allEnvDataList_i[z_index];
			//ui.textEdit_CloudLog->insertPlainText(allEnvDataList_i[picname_index] + QStringLiteral("：") + res[allEnvDataList_i[picname_index]] + QStringLiteral("\n"));
		}
	}

	workbook->dynamicCall("Close (Boolean)", false);
	excel.dynamicCall("Quit()");
	OleUninitialize();

	return res;
}

std::vector<std::vector<QString>> MainControl::LoadImageJsonDescription()
{
	QString imagedes = ui.label_Bird_ResDir->text() + QStringLiteral("/图像信息.json");
	std::vector<std::vector<QString>> res;


	boost::property_tree::ptree pt;
	// 读取.json配置文件  utf8格式的

	std::ifstream is;
	is.open(imagedes.toLocal8Bit().data(), std::ios::in);
	if (is.is_open())
	{
		try {
			read_json(is, pt);          //parse json
			is.close();
		}
		catch (...) {
			QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("航行路径.json读取失败！"), 0);
			return res;
		}
		is.close();
	}
	else
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("图像信息打开失败！"), 0);
		return res;
	}

	try
	{
		BOOST_FOREACH(boost::property_tree::ptree::value_type &cvt, pt)
		{
			std::vector<QString> img;
			img.push_back(QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("name")).c_str()));
			img.push_back(QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("偏航角")).c_str()));
			img.push_back(QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("俯仰角")).c_str()));
			img.push_back(QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("经度")).c_str()));
			img.push_back(QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("纬度")).c_str()));
			img.push_back(QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("高度")).c_str()));
			img.push_back(QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("时间")).c_str()));
			res.push_back(img);
		}
	}
	catch (...)
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("图像信息读取失败！"), 0);
		return res;
	}


	return res;
}

std::vector<std::tuple<QString, QString>> MainControl::LoadFlightJsonInfomation()
{
	QString filename = ui.lineEdit_FlightInfomation->text();
	std::vector<std::tuple<QString, QString>> res;


	boost::property_tree::ptree pt;
	// 读取.json配置文件  utf8格式的

	std::ifstream is;
	is.open(filename.toLocal8Bit().data(), std::ios::in);
	if (is.is_open())
	{
		try {
			read_json(is, pt);          //parse json
			is.close();
		}
		catch (...) {
			QMessageBox::information(this, "鸿业提示", "航行路径读取失败！", 0);
			return res;
		}
		is.close();
	}
	else
	{
		QMessageBox::information(this, "鸿业提示", "航行路径打开失败！", 0);
		return res;
	}

	try
	{
		BOOST_FOREACH(boost::property_tree::ptree::value_type &cvt, pt)
		{
			QString position = QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("position")).c_str());
			QString pointTime = QString::fromUtf8(cvt.second.get<std::string>(ChartSetConv::C2W("pointTime")).c_str());
			res.push_back(std::make_tuple(position, pointTime));
		}
	}
	catch (...)
	{
		QMessageBox::information(this, "鸿业提示", "航行路径读取失败！", 0);
		return res;
	}

	return res;
}

void MainControl::closeEvent(QCloseEvent *event)
{
	cloud_process_->close();
	brid_process_->close();
	SaveSetting();
	QMainWindow::closeEvent(event);
}

void MainControl::CloudGetCloudsPath()
{
	QString path = QFileDialog::getOpenFileName(this, QStringLiteral("请选择点云文件..."), QFileInfo(ui.lineEdit_Cloud_CloudPath->text()).absoluteDir().path(), QStringLiteral("Clouds File(*.las)"));
	if (path.length() == 0) {
		return;
	}

	QString res_dir = QFileInfo(path).absoluteDir().path() + QStringLiteral("/") + QFileInfo(path).baseName();
	ui.lineEdit_Cloud_CloudPath->setText(path);
	ui.label_Cloud_ResultDir->setText(res_dir);
}

void MainControl::CloudGetTowersDir()
{
	QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择铁塔目录..."), ui.lineEdit_Cloud_TowerDir->text());
	if (path.length() == 0) {
		return;
	}

	ui.lineEdit_Cloud_TowerDir->setText(path);
}

void MainControl::CloudGetClassDir()
{
	QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择样本目录..."), ui.lineEdit_Cloud_ClassDir->text());
	if (path.length() == 0) {
		return;
	}

	ui.lineEdit_Cloud_ClassDir->setText(path);
}

void MainControl::LoadAirRouteInfo()
{
	QString filename = ui.label_Cloud_ResultDir->text() + QStringLiteral("/线路信息.txt");

	QFile file(filename);
	// Trying to open in WriteOnly and Text mode
	if (!file.open(QFile::ReadOnly | QFile::Text))
	{
		return;
	}

	QString Circuit_Name, Circuit_Range, Kv, Date;
	QTextStream out(&file);
	out >> /*QStringLiteral("线路名称=") >>*/ Circuit_Name;
	out >> /*QStringLiteral("分段区间=") >>*/ Circuit_Range ;
	out >> /*QStringLiteral("电压等级=") >>*/ Kv ;
	out >> /*QStringLiteral("采集日期=") >>*/ Date ;

	Circuit_Name = Circuit_Name.right(Circuit_Name.length() - Circuit_Name.indexOf(QStringLiteral("=")) -1);
	Circuit_Range = Circuit_Range.right(Circuit_Range.length() - Circuit_Range.indexOf(QStringLiteral("=")) - 1);
	Kv = Kv.right(Kv.length() - Kv.indexOf(QStringLiteral("="))- 1);
	Date = Date.right(Date.length() - Date.indexOf(QStringLiteral("=")) - 1);

	file.close();
}

void MainControl::SaveAirRouteInfo()
{
	QString filename = ui.label_Cloud_ResultDir->text() + QStringLiteral("/线路信息.txt");

	QFile file(filename);
	// Trying to open in WriteOnly and Text mode
	if (!file.open(QFile::WriteOnly | QFile::Text))
	{
		return;
	}

	QTextStream out(&file);
	out << QStringLiteral("线路名称=") << ui.lineEdit_Circuit_Name->text() << QStringLiteral("\n");
	out << QStringLiteral("分段区间=") << ui.lineEdit_Circuit_Range->text() << QStringLiteral("\n");
	out << QStringLiteral("电压等级=") << ui.lineEdit_Kv->text() << QStringLiteral("\n");
	out << QStringLiteral("采集日期=") << ui.lineEdit_Date->text() << QStringLiteral("\n");
	file.flush();
	file.close();
}

void MainControl::RefreshProj()
{
	QString projname = ServerFunc::GetProjectName();
	QString projcode = ServerFunc::GetProjectCode();
	if (projcode.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("请登录鸿程后切换当前项目。"));
		return;
	}
	ui.label_projname->setText(projname);
	ui.label_projcode->setText(projcode);
}

void MainControl::SubmitCloudWarningReport()
{
	QString output_dir = ui.label_Cloud_ResultDir->text();
	//QTextCodec* codec = QTextCodec::codecForName("UTF-8");


	// 准备数据
	QString resdir = ui.label_Cloud_ResultDir->text();
	QString flightpath_path = resdir + QStringLiteral("/飞行路径.json");
	std::vector<QString> paths;
	paths.push_back(resdir + QStringLiteral("/点云检测结果.pdf"));
	paths.push_back(resdir + QStringLiteral("/点云检测结果.json"));
	if (QFile::exists(flightpath_path))
		paths.push_back(flightpath_path);

	QString zip_file = ui.label_Cloud_ResultDir->text() + QStringLiteral("/") + QDir(ui.label_Cloud_ResultDir->text()).dirName() + QStringLiteral(".zip");
	ArchiveFiles(zip_file, paths);

	// 组包提交数据
	std::shared_ptr<QNetworkAccessManager> proc_manager = std::make_shared<QNetworkAccessManager>();
	std::shared_ptr<QHttpMultiPart> multiPart = std::make_shared<QHttpMultiPart>(QHttpMultiPart::FormDataType);
	
	
	QHttpPart ProjectCode;
	ProjectCode.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QStringLiteral("form-data; name=\"ProjectCode\"")));
	ProjectCode.setBody(ServerFunc::GetProjectCode().toUtf8());

	QHttpPart zipFile;
	zipFile.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/zip")));
	zipFile.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QStringLiteral("form-data; name=\"File\"; filename=\"Archive.zip\"")));
	QFile file(zip_file);
	file.open(QIODevice::ReadOnly);
	zipFile.setBodyDevice(&file);


	multiPart->append(ProjectCode);
	multiPart->append(zipFile);


	QNetworkReply *reply = proc_manager->post(QNetworkRequest(QUrl(QStringLiteral("http://") + ui.lineEdit_server_ip->text() + QStringLiteral("/api/gisdata/process"))), multiPart.get());
	QByteArray responseData;
	QEventLoop eventLoop;
	connect(proc_manager.get(), SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
	eventLoop.exec();
	responseData = reply->readAll();
	QString response_str = QString::fromUtf8(responseData.data());

	file.close();
}


/*    测试代码
QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
std::vector<QString> paths;
paths.push_back(dir + QStringLiteral("/新建文本文档.txt"));
paths.push_back(dir + QStringLiteral("/线路信息.txt"));
paths.push_back(dir + QStringLiteral("/data-20181220-042719检测结果.pdf"));
paths.push_back(dir + QStringLiteral("/gui-config.json"));
setWindowTitle(dir);
ArchiveFiles(dir + QStringLiteral("/我a.zip"), paths);
return;
*/
void MainControl::ArchiveFiles(QString name, std::vector<QString> paths)
{
	struct zip_t *zip = zip_open(name.toLocal8Bit().data(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');

	QString filename = QFileInfo(name).fileName();
	QString basename = QFileInfo(name).baseName();
	for (int i = 0; i < paths.size(); ++i)
	{
		zip_entry_open(zip, (basename + QStringLiteral("/") + QFileInfo(paths[i]).fileName()).toLocal8Bit().data());
		{
			zip_entry_fwrite(zip, paths[i].toLocal8Bit().data());
		}
		zip_entry_close(zip);
	}
	zip_close(zip);
}

void MainControl::CloudUpLoadProj()
{

	QString projpath = ui.lineEdit_projpath->text();
	QString projdir = projpath + QStringLiteral("/") + ServerFunc::GetProjectCode() +  QStringLiteral("/mds");
	QString output_dir = ui.label_Cloud_ResultDir->text();

	
	if (!QDir(projpath).exists())
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("项目路径不存在或者没有访问权限。"));
		return;
	}
	if (!QDir(output_dir + QStringLiteral("/ground")).exists())
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("请先分析点云。"));
		return;
	}
	if (!QFile::exists(output_dir + QStringLiteral("/点云检测结果.pdf")))
	{
		
		return;
	}

	if (!QDir(projdir).exists())
	{
		QDir().mkpath(projdir);
		if (!QDir(projdir).exists())
		{
			QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("项目目录创建失败，请手动创建。"));
			return;
		}
	}

	FileUtil::CopyDirectory(output_dir + QStringLiteral("/ground"), projdir + QStringLiteral("/ground"));
	FileUtil::CopyDirectory(output_dir + QStringLiteral("/lines"), projdir + QStringLiteral("/lines"));
	FileUtil::CopyDirectory(output_dir + QStringLiteral("/others"), projdir + QStringLiteral("/others"));
	FileUtil::CopyDirectory(output_dir + QStringLiteral("/towers"), projdir + QStringLiteral("/towers"));
	FileUtil::CopyDirectory(output_dir + QStringLiteral("/vegets"), projdir + QStringLiteral("/vegets"));

	SubmitCloudWarningReport();
	
}

void MainControl::CloudRun()
{
	ui.textEdit_CloudLog->clear();
	
	QStringList args;

	QString app_dir = QApplication::applicationDirPath();
	QString las_path = ui.lineEdit_Cloud_CloudPath->text();
	if (!QFile::exists(las_path))
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("点云路径不存在。"), 0);
		return;
	}
	QString class_path = ui.lineEdit_Cloud_ClassDir->text() + QStringLiteral("/config.xml");
	if (!QFile::exists(class_path))
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("样本目录中不存在训练文件(config.xml)。"), 0);
		return;
	}
	QString tower_dir = ui.lineEdit_Cloud_TowerDir->text();
	if (!QDir().exists(tower_dir))
	{
		QMessageBox::information(this, QStringLiteral("鸿业提示"), QStringLiteral("请选则铁塔目录。"), 0);
		return;
	}

	
	statusBar()->showMessage(QFileInfo(las_path).baseName() + QStringLiteral(".las分析中..."), 0);

	FileUtil::ReMakeDir(ui.label_Cloud_ResultDir->text());
	SaveAirRouteInfo();


	QStringList mycmd;
	mycmd << QStringLiteral("--cmdtype") << QStringLiteral("poscorrect") << QStringLiteral("--inputfile") << las_path << QStringLiteral("--classdir") << ui.lineEdit_Cloud_ClassDir->text() <<
		QStringLiteral("--method") << QString::number(2) << QStringLiteral("--exceldir") << tower_dir << QStringLiteral("--overrideExcel") << QStringLiteral("0");



	args << mycmd;
	ui.pushButton_cloud_run->setEnabled(false);

	cloud_process_->start(app_dir + QStringLiteral("/PointCloudTool.exe"), args);
	cloud_process_->waitForStarted();

	while (!ui.pushButton_cloud_run->isEnabled())
	{
		QApplication::processEvents();
	}


	WriteFlightPath(ui.label_Cloud_ResultDir->text() + QStringLiteral("/飞行路径.json"));

	statusBar()->showMessage(QFileInfo(las_path).baseName() + QStringLiteral(".las分析完成。 返回值=") + QString::number(cloud_rescode_) + QStringLiteral("，是否异常退出=") + QString::number(cloud_exitstatus_), 0);
}

void MainControl::GetFlightInfomationPath()
{
	QString path = QFileDialog::getOpenFileName(this, QStringLiteral("请选择航行路径文件..."), QFileInfo(ui.lineEdit_FlightInfomation->text()).absoluteDir().path(), QStringLiteral("Flight Infomation(*.xlsx *.xls)"));
	if (path.length() == 0) {
		return;
	}

	ui.lineEdit_FlightInfomation->setText(path);
}

void MainControl::CloudReadyReadStandardOutput()
{
	ui.textEdit_CloudLog->insertPlainText(QTextCodec::codecForName("GB2312")->toUnicode(cloud_process_->readAll()));
	ui.textEdit_CloudLog->moveCursor(QTextCursor::End);
}

void MainControl::BridReadyReadStandardOutput()
{
	ui.textEdit_BridLog->insertPlainText(QTextCodec::codecForName("GB2312")->toUnicode(brid_process_->readAll()));
	ui.textEdit_BridLog->moveCursor(QTextCursor::End);
}

void MainControl::CloudFinished(int exitcode, QProcess::ExitStatus status)
{
	cloud_rescode_ = exitcode;
	cloud_exitstatus_ = status;
	ui.pushButton_cloud_run->setEnabled(true);
}

void MainControl::BridFinished(int exitcode, QProcess::ExitStatus status)
{
	brid_rescode_ = exitcode;
	brid_exitstatus_ = status;
	ui.pushButton_brid_run->setEnabled(true);
}

void MainControl::CloudOpenResultDir()
{
	QDesktopServices::openUrl(QUrl(QStringLiteral("file:") + ui.label_Cloud_ResultDir->text(), QUrl::TolerantMode));
}

void MainControl::BirdRun()
{
	ui.textEdit_BridLog->clear();
}
