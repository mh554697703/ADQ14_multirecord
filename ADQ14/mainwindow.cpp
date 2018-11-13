﻿#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ADQAPI.h"
#include "threadstore.h"

#include <QDebug>
#include <QFileDialog>
#include <QSettings>

#include <QMessageBox>
#include <QFile>
#include <QDateTime>
#include <QtCore>
#include <QLabel>

#include <QDesktopServices>
#include <QApplication>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	QDir dirs;
	QString path = dirs.currentPath()+"/"+"214settings.ini";				//获取初始默认路径，并添加默认配置文件
	m_setfile.test_create_file(path);									//检查settings.ini是否存在，若不存在则创建
	m_setfile.readFrom_file(path);										//读取settings.ini文件
	mysetting = m_setfile.get_setting();								//mysetting获取文件中的参数
	initial_plotValue();												//初始的绘图显示参数

	creatleftdock();													//左侧栏
	creatqwtdock();														//曲线栏
	Create_statusbar();													//状态栏

	adq_cu = CreateADQControlUnit();
    ADQControlUnit_EnableErrorTrace(adq_cu, LOG_LEVEL_INFO, ".");
    connectADQDevice();								//连接采集卡设备
	timer_trigger_waiting = new QTimer(this);							//用于设定触发等待超时时间，在do—while循环中，如果超时还没有触发，就跳出

    timer_judge = new QTimer(this);
    connect(timer_judge,SIGNAL(timeout()),this,SLOT(timer_count()));
    connect(timer_judge,SIGNAL(timeout()),this,SLOT(judge_collect_condition()));

	//初始状态值											
	onecollect_over = true;
	thread_enough = true;
	success_configure = true;
	stopped = true;														//初始状态，未进行数据采集
	num_running = 0;													//运行的数据存储线程数为0
	need_instruct = true;

	connect(&threadA, SIGNAL(store_finish()),this,SLOT(receive_storefinish()));
	connect(&threadB, SIGNAL(store_finish()),this,SLOT(receive_storefinish()));
	connect(&threadC, SIGNAL(store_finish()),this,SLOT(receive_storefinish()));
	connect(&threadD, SIGNAL(store_finish()),this,SLOT(receive_storefinish()));
	collect_state->setText(QString::fromLocal8Bit("未进行采集"));
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::initial_plotValue()
{
	m_paraValue.hide_grid = false;
	m_paraValue.showA = true;
	m_paraValue.showB = true;
	m_paraValue.countNum = false;
	m_paraValue.echoDistance = true;
}

//创建左侧显示栏
void MainWindow::creatleftdock(void)
{
	dockleft_dlg = new informationleft(this);
	dockWidget = new QDockWidget;
	dockWidget->setWidget(dockleft_dlg);
	dockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
//	dockWidget->setWindowTitle(QString::fromLocal8Bit("左侧栏"));
	addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

	dockleft_dlg->set_currentAngle(mysetting.start_azAngle);
	dockleft_dlg->set_groupNum(mysetting.angleNum);
	dockleft_dlg->set_groupcnt(0);

	if(mysetting.isSingleCh)
	{
		FileName_1 = mysetting.dataFileName_Prefix + "_ch1_" + mysetting.dataFileName_Suffix + ".wld";
		dockleft_dlg->set_filename1(FileName_1);
        dockleft_dlg->set_filename2(nullptr);
	}
	else
	{
		FileName_A = mysetting.dataFileName_Prefix + "_chA_" + mysetting.dataFileName_Suffix + ".wld";
		FileName_B = mysetting.dataFileName_Prefix + "_chB_" + mysetting.dataFileName_Suffix + ".wld";
		dockleft_dlg->set_filename1(FileName_A);
		dockleft_dlg->set_filename2(FileName_B);
	}
}

//创建曲线显示部分
void MainWindow::creatqwtdock(void)
{
	plot1show = false;
	plot2show = false;
	if(m_paraValue.showA)
	{
		plot1show = true;
		plotWindow_1 = new PlotWindow(this);
		dockqwt_1 = new QDockWidget;
		dockqwt_1->setWidget(plotWindow_1);
		dockqwt_1->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
		addDockWidget(Qt::RightDockWidgetArea,dockqwt_1);
		plotWindow_1->setMaxX(mysetting.sampleNum,mysetting.sampleFreq,m_paraValue.countNum);
		connect(dockqwt_1,&QDockWidget::topLevelChanged,this,&MainWindow::dockview_ct1);
		if(mysetting.isSingleCh)
		{
//			dockqwt_1->setWindowTitle(QString::fromLocal8Bit("CH1"));
			plotWindow_1->set_titleName("CH1");
		}
		else
		{
//			dockqwt_1->setWindowTitle(QString::fromLocal8Bit("CHA"));
			plotWindow_1->set_titleName("CHA");
		}
		plotWindow_1->set_grid(m_paraValue.hide_grid);
	}

	if((!mysetting.isSingleCh)&&m_paraValue.showB)
	{
		plot2show = true;
		plotWindow_2 = new PlotWindow(this);				//创建plotWindow_2的图形区域
		dockqwt_2 = new QDockWidget;
		dockqwt_2->setWidget(plotWindow_2);
		dockqwt_2->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
		addDockWidget(Qt::RightDockWidgetArea,dockqwt_2);
		plotWindow_2->setMaxX(mysetting.sampleNum,mysetting.sampleFreq,m_paraValue.countNum);
		connect(dockqwt_2,&QDockWidget::topLevelChanged,this,&MainWindow::dockview_ct2);
//		dockqwt_2->setWindowTitle(QString::fromLocal8Bit("CHB"));
		plotWindow_2->set_titleName("CHB");					//双通道B的名称
		plotWindow_2->set_grid(m_paraValue.hide_grid);
	}
	setPlotWindowVisible();
}

//创建状态栏
void MainWindow::Create_statusbar()
{
	bar = ui->statusBar;											//获取状态栏
	QFont font("Microsoft YaHei UI",9);								//设置状态栏字体大小
	bar->setFont(font);

	ADQ_state = new QLabel;											//新建标签
	ADQ_state->setMinimumSize(115,22);								//设置标签最小尺寸
	ADQ_state->setAlignment(Qt::AlignLeft);							//设置对齐方式，左侧对齐
	bar->addWidget(ADQ_state);

	motor_state = new QLabel;
	motor_state->setMinimumSize(95,22);
	motor_state->setAlignment(Qt::AlignLeft);
	bar->addWidget(motor_state);

	collect_state = new QLabel;
	collect_state->setMinimumSize(85,22);
	collect_state->setAlignment(Qt::AlignLeft);
	bar->addWidget(collect_state);

	storenum = new QLabel;
	storenum->setMinimumSize(965,22);
	storenum->setAlignment(Qt::AlignLeft);
	bar->addWidget(storenum);
}

//查找并连接ADQ214设备
void MainWindow::connectADQDevice()
{
	int n_of_devices = ADQControlUnit_FindDevices(adq_cu);			//找到所有与电脑连接的ADQ，并创建一个指针列表，返回找到设备的总数
	int n_of_failed = ADQControlUnit_GetFailedDeviceCount(adq_cu);
    int n_of_ADQ14 = ADQControlUnit_NofADQ14(adq_cu);				//返回找到ADQ214设备的数量
    qDebug() << "n_of_ADQ14 = " << n_of_ADQ14;
    if((n_of_failed > 0)||(n_of_devices == 0))
	{
		ADQ_state->setText(QString::fromLocal8Bit("采集卡未连接"));
		return;
	}
    if(n_of_ADQ14 != 0)
		ADQ_state->setText(QString::fromLocal8Bit("采集卡已连接"));
    int api_rev = ADQAPI_GetRevision();
    int* fw_rev = ADQ_GetRevision(adq_cu, 1);
    char* serial_number = ADQ_GetBoardSerialNumber(adq_cu, 1);
    char* product_name = ADQ_GetBoardProductName(adq_cu, 1);
    qDebug()<< "nAPI revision: "<<api_rev;
    qDebug()<< "Firmware revision: "<<fw_rev[0];
    qDebug()<< "Board serial number: "<<serial_number;
    qDebug()<< "Board product name: "<<product_name;
}

void MainWindow::dockview_ct1(bool topLevel)				//用于通道1全屏
{
	if(topLevel)
		dockqwt_1->showMaximized();
}

void MainWindow::dockview_ct2(bool topLevel)				//用于通道2全屏
{
	if(topLevel)
		dockqwt_2->showMaximized();
}

void MainWindow::setPlotWindowVisible()						//设置绘图窗口的尺寸
{
	int w = width();
	int h = height();
	w = w - 265, h = (h-25-42-22-25)/2;						//高度：菜单25，工具42，状态22，左侧边栏宽度260，再空去25的高度
	if(m_paraValue.showA)
	{
		plotWindow_1->setFixedSize(w,h);
		plotWindow_1->setMaximumSize(1920,1080);			//绘图窗口的最大尺寸
	}

	if((!mysetting.isSingleCh)&&m_paraValue.showB)
	{
		plotWindow_2->setFixedSize(w,h);
		plotWindow_2->setMaximumSize(1920,1080);
	}
}

void MainWindow::resizeEvent(QResizeEvent *event)			//主窗口大小改变时，保证绘图窗口填满
{
	setPlotWindowVisible();
}

//连接USB采集卡
void MainWindow::on_action_searchDevice_triggered()
{
	connectADQDevice();
}

//打开数据存储路径
void MainWindow::on_action_open_triggered()
{
	Create_DataFolder();
	QDesktopServices::openUrl(QUrl::fromLocalFile(mysetting.DatafilePath));
}

//打开关于对话框
void MainWindow::on_action_about_triggered()
{
	HelpDialog = new helpDialog(this);
	HelpDialog->exec();
	delete HelpDialog;
}

//打开参数设置对话框
void MainWindow::on_action_set_triggered()
{
	ParaSetDlg = new paraDialog(this);
	ParaSetDlg->init_setting(mysetting,stopped);					//mysetting传递给设置窗口psetting
	ParaSetDlg->initial_para();										//参数显示在设置窗口上，并连接槽
	ParaSetDlg->on_checkBox_autocreate_datafile_clicked();			//更新文件存储路径
	if (ParaSetDlg->exec() == QDialog::Accepted)					// 确定键功能
	{
		mysetting =	ParaSetDlg->get_settings();						//mysetting获取修改后的参数
		dockleft_dlg->set_currentAngle(mysetting.start_azAngle);	//更新左侧栏
		dockleft_dlg->set_groupNum(mysetting.angleNum);
		dockleft_dlg->set_groupcnt(0);
		if(mysetting.isSingleCh)
		{
			FileName_1 = mysetting.dataFileName_Prefix + "_ch1_" + mysetting.dataFileName_Suffix + ".wld";
			dockleft_dlg->set_filename1(FileName_1);
            dockleft_dlg->set_filename2(nullptr);
		}
		else
		{
			FileName_A = mysetting.dataFileName_Prefix + "_chA_" + mysetting.dataFileName_Suffix + ".wld";
			FileName_B = mysetting.dataFileName_Prefix + "_chB_" + mysetting.dataFileName_Suffix + ".wld";
			dockleft_dlg->set_filename1(FileName_A);
			dockleft_dlg->set_filename2(FileName_B);
		}
		refresh();					//更新绘图窗口
	}
	delete ParaSetDlg;
}

//参数设置对话框关闭后，对绘图曲线部分进行更新
void MainWindow::refresh()
{
	if(plot1show)
	{
		delete plotWindow_1;
		delete dockqwt_1;
	}
	if(plot2show)
	{
		delete plotWindow_2;
		delete dockqwt_2;
	}
	creatqwtdock();
}

//采集说明
void MainWindow::on_action_collect_instruct_triggered(bool checked)
{
	need_instruct = checked;
}

//打开绘图设置对话框
void MainWindow::on_action_view_triggered()
{
	PlotDialog = new plotDialog(this);				//绘图设置对话框
	PlotDialog->dialog_show(m_paraValue,mysetting.isSingleCh);
	bool defaulA = m_paraValue.showA;
	bool defaulB = m_paraValue.showB;
	if(PlotDialog->exec() == QDialog::Accepted)
	{
		m_paraValue = PlotDialog->get_settings();
		if((defaulA == m_paraValue.showA)&&(defaulB == m_paraValue.showB))
		{
			if(m_paraValue.showA)
			{
				plotWindow_1->set_grid(m_paraValue.hide_grid);
				plotWindow_1->update_xAxis(m_paraValue.countNum);
			}

			if((!mysetting.isSingleCh)&&m_paraValue.showB)
			{
				plotWindow_2->set_grid(m_paraValue.hide_grid);
				plotWindow_2->update_xAxis(m_paraValue.countNum);
			}
		}
		else
			refresh();
	}
	delete PlotDialog;
}

//采集菜单中的开始按钮
void MainWindow::on_action_start_triggered()
{
	if((num_running == 4)||(stopped == false))		//检查存储线程是否完成数据存储
	{
		hintInfo_handle(1);
		return;
	}
	if(mysetting.angleNum == 0)						//方向数为0时，不采集
	{
		hintInfo_handle(2);
		return;
	}

	n_sample_skip = 1;					//采样间隔设为1，表示无采样间隔
    if(ADQ_SetSampleSkip(adq_cu,1,n_sample_skip) == 0)
	{
        qDebug()<<"SetSampleSkip";
		hintInfo_handle(3);
		return;
	}

	success_configure = adq_para_set();	//设置采集卡参数
    qDebug() << "success_configure = " << success_configure;
	if(success_configure == true)		//采集卡配置成功
	{
		//增加采集记录和说明
		QString text;
		text.clear();
		if(need_instruct)
		{
			bool ok;
			text = QInputDialog::getText(this,QString::fromLocal8Bit("采集说明"),
												 QString::fromLocal8Bit("请输入记录文字"),
                                                 QLineEdit::Normal,QString::fromLocal8Bit(nullptr),&ok);
	//			if(ok&&(!text.isEmpty()))
	//				qDebug() << "OK";
		}
		m_setfile.updatelogFile(text);

		direction_intervalNum = mysetting.direct_intervalTime * FREQUENCY_OF_JUDGE;
		dI_timer_counter = direction_intervalNum;		//为了第一次

		Create_DataFolder();			//创建数据存储文件夹
		num_collect = 0;
		locus_error = false;
		stopped = false;				//stopped设置为false
		notrig_signal = false;
		thread_enough = true;
        timer_judge->start(PERIOD_OF_JUDGE);
	}
	else											//采集卡配置失败
		hintInfo_handle(4);
}

//采集停止的提示信息和处理
void MainWindow::hintInfo_handle(int controlNum)
{
	if(controlNum >4)
	{
        timer_judge->stop();
		onecollect_over = true;
		collect_reset();
		collect_state->setText(QString::fromLocal8Bit("采集结束"));
	}
	switch (controlNum) {
	case 1:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("数据存储尚未完成"));
		break;
	case 2:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集组数为0"));
		break;
	case 3:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集卡连接异常，请重新连接"));
		break;
	case 4:
        ADQ_DisarmTrigger(adq_cu,1);
        ADQ_MultiRecordClose(adq_cu,1);
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集卡设置失败"));
		break;
	case 5:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集已停止"));
		break;
	case 6:
		timer_trigger_waiting->stop();
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集卡未接收到触发信号"));
		break;
	case 7:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集卡数据采集失败"));
		break;
	case 8:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集卡出现故障，请重新启动连接采集卡"));
		break;
	case 9:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("单文件数据量较大，存储较慢，请适当降低转速"));
		break;
	case 10:
		QMessageBox::information(this,QString::fromLocal8Bit("提示"),QString::fromLocal8Bit("采集结束"));
		break;
	default:
		break;
	}
}

//采集菜单中的停止按钮
void MainWindow::on_action_stop_triggered()
{
	stopped = true;
}

void MainWindow::on_action_help_triggered()
{
	QDir help;
	QProcess *mp_helpProcess = new QProcess(this);
	QStringList argument(help.currentPath()+"/helphtml.CHM");
	mp_helpProcess->start("hh.exe",argument);				//chm格式可用windows自带的hh.exe进行打开
}

//数据存储文件夹的创建
void MainWindow::Create_DataFolder()
{
	QDir mypath;
	if(!mypath.exists(mysetting.DatafilePath))		//如果文件夹不存在，创建文件夹
		mypath.mkpath(mysetting.DatafilePath);
}

//方向间、圆周间间隔计数
void MainWindow::timer_count()
{
	dI_timer_counter++;
	cI_timer_counter++;
}

//定时判断是否进行下一组采集
void MainWindow::judge_collect_condition()
{
	if((dI_timer_counter >= direction_intervalNum)
            &&(onecollect_over == true))
	{
		onecollect_over = false;						//单次采集开始
		dI_timer_counter = 0;							//判断次数清零

		collect_state->setText(QString::fromLocal8Bit("数据采集中..."));
		success_configure = adq_collect();
		qDebug() << "main 4adq_collect";
		if(notrig_signal)
		{
			hintInfo_handle(6);
			return;
		}
		qDebug() << "main 5trig_signal";
		if(success_configure == true)				//采集卡采集成功
		{
			if(mysetting.isSingleCh)				//数据上传并存储
				single_upload_store();
			else
				double_upload_store();

			if(success_configure == true)
			{
				if(thread_enough == true)
				{
					collect_state->setText(QString::fromLocal8Bit("数据上传成功..."));
					update_collect_number();		//更新当前采集信息
					//判断是否完成设置组数
					qDebug() << "main 7update_collectNum";
					if((num_collect >= mysetting.angleNum)||(stopped == true))
						hintInfo_handle(10);
					onecollect_over = true;
				}
				else
					hintInfo_handle(9);
			}
			else
				hintInfo_handle(8);
		}
		else
			hintInfo_handle(7);
	}
}

//采集卡参数设置
bool MainWindow::adq_para_set()
{
	int trig_mode = mysetting.trigger_mode;
    if(ADQ_SetTriggerMode(adq_cu,1,trig_mode) == 0)
		return false;
    if(trig_mode == 4)                          //内部触发
    {
        if(ADQ_SetInternalTriggerPeriod(adq_cu,1,1000000) == 0)
            return false;
        if(ADQ_SetConfigurationTrig(adq_cu,1,0x41,0,0) == 0)  //示例中没用到
            return false;
    }
    else
        if(trig_mode == 3)						//电平触发
        {
            qDebug() << "dianpingchufa!";
            qint16 trig_level = mysetting.trigLevel;
            qDebug() << "trig_level" << trig_level;
            if(ADQ_SetLvlTrigLevel(adq_cu,1,trig_level) == 0)
                return false;
            int trigger_edge = 1;					//触发边沿 -> 上升沿
            if(ADQ_SetLvlTrigEdge(adq_cu,1,trigger_edge) == 0)
                return false;
            int trig_channel = 1;				  //触发通道:1通道A,2通道B
            if(ADQ_SetLvlTrigChannel(adq_cu,1,trig_channel) == 0)
                return false;
        }

	int clock_source = 0;						//时钟源选择0，内部时钟，内部参考
    if(ADQ_SetClockSource(adq_cu,1,clock_source) == 0)
		return false;

    //ADQ14示例新增一个函数ADQ_MultiRecordSetChannelMask()

    if(mysetting.isPreTrig)
    {
        if(ADQ_SetPreTrigSamples(adq_cu,1,mysetting.Pre_OR_HoldOff_Samples) == 0)  // ADQ14-4C/2C: 4 samples
            return false;
    }
    else
    {
        if(ADQ_SetTriggerHoldOffSamples(adq_cu,1,mysetting.Pre_OR_HoldOff_Samples) == 0)
            return false;
    }

	number_of_records = mysetting.plsAccNum;	//脉冲数
	samples_per_record = mysetting.sampleNum;	//采样点数
    if(ADQ_MultiRecordSetup(adq_cu,1,number_of_records,samples_per_record) == 0)
		return false;
	return true;
}

//采集卡触发进行采集、并更新左侧采集文件和方位信息
bool MainWindow::adq_collect()
{
    if(ADQ_DisarmTrigger(adq_cu,1) == 0)			//Disarm unit
		return false;
    if(ADQ_ArmTrigger(adq_cu,1) == 0)			//Arm unit
		return false;

	dockleft_dlg->set_groupcnt(num_collect+1);					//进度条更新
	if(mysetting.isSingleCh)										//单通道文件名更新
		dockleft_dlg->set_filename1(FileName_1);
	else															//双通道文件名更新
	{
		dockleft_dlg->set_filename1(FileName_A);
		dockleft_dlg->set_filename2(FileName_B);
	}

	QDateTime collectTime = QDateTime::currentDateTime();			//采集开始时间
	timestr = collectTime.toString("yyyy/MM/dd hh:mm:ss");
	qDebug() << "main 3Please trig your device to collect data. mysetting.trig_mode = " << mysetting.trigger_mode;
	int trigged;
	if(mysetting.trigger_mode == 3)
		timer_trigger_waiting->start(LEVELTRIG_WAIT_TIME);
	else
		timer_trigger_waiting->start(EXTERNTRIG_WATETIME);
	do
	{
		QCoreApplication::processEvents();
        trigged = ADQ_GetAcquiredAll(adq_cu,1);					//Trigger unit   Use GetAcquiredAll() instead
		if(timer_trigger_waiting->remainingTime() == 0)
		{
			notrig_signal = true;
			return false;
		}
	}while(trigged == 0);
	timer_trigger_waiting->stop();
	qDebug() << "main 4Device trigged.";

//	if((mysetting.step_azAngle != 0)&&((num_collect+1)< mysetting.angleNum)&&(stopped == false))
//		PortDialog->CW_Rotate(mysetting.step_azAngle);
	return true;
}

//单通道数据上传、存储和显示
void MainWindow::single_upload_store()
{
	qDebug() << "main 6Collecting data,plesase wait...";
	collect_state->setText(QString::fromLocal8Bit("数据上传中..."));
	void *target_buffers[1];
	qint16 *rd_data1 = new qint16[samples_per_record*number_of_records];
	qint16 *need_rd1 = new qint16[samples_per_record];
	target_buffers[0] = need_rd1;
	for(uint i = 0;i < number_of_records;i++)
	{
		QCoreApplication::processEvents();
		int rn1 = 0;
        ADQ_GetData(adq_cu,1,target_buffers,samples_per_record,
					   2,i,1,0x01,0,samples_per_record,0x00);
		for(uint j = 0;j < samples_per_record;j++)
		{
			rd_data1[i*samples_per_record + rn1] = *(need_rd1 + j);
			rn1++;
		}
	}
	delete[] need_rd1;
//	QApplication.processEvents();
//	if(ADQ214_GetData(adq_cu,1,target_buffers,samples_per_record*number_of_records,
//					 2,0,number_of_records,0x01,0,samples_per_record,0x00) == 0)
//	{
//		delete[] rd_data1;
//		success_configure = false;
//		return;
//	}

	direction_angle = (mysetting.start_azAngle + num_collect*mysetting.step_azAngle)%360;
	if(plot1show)
		plotWindow_1->datashow(rd_data1,mysetting.plsAccNum);//绘图窗口显示最后一组脉冲
	if(!threadA.isRunning())
	{
		num_running++;										//线程数加1，状态栏显示线程数
		storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
		threadA.fileDataPara(mysetting);					//mysetting值传递给threadstore
		threadA.otherpara(timestr,direction_angle);			//时间，方位角传递给threadstore
		threadA.s_memcpy(rd_data1);							//采样数据传递给threadstore
		threadA.start();									//启动threadstore线程
	}
	else
		if(!threadB.isRunning())
		{
			num_running++;
			storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
			threadB.fileDataPara(mysetting);
			threadB.otherpara(timestr,direction_angle);
			threadB.s_memcpy(rd_data1);
			threadB.start();
		}
		else
			if(!threadC.isRunning())
			{
				num_running++;
				storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
				threadC.fileDataPara(mysetting);
				threadC.otherpara(timestr,direction_angle);
				threadC.s_memcpy(rd_data1);
				threadC.start();
			}
			else
				if(!threadD.isRunning())
				{
					num_running++;
					storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
					threadD.fileDataPara(mysetting);
					threadD.otherpara(timestr,direction_angle);
					threadD.s_memcpy(rd_data1);
					threadD.start();
				}
				else										//四个线程都在运行时，停止采集
				{
					delete[] rd_data1;
					thread_enough = false;
					return;
				}

	delete[] rd_data1;
}

//双通道数据上传、存储和显示
void MainWindow::double_upload_store()
{
	qDebug() << "main 6Collecting data,plesase wait...";
	collect_state->setText(QString::fromLocal8Bit("数据上传中..."));
	void *target_buffers[2];
	qint16 *rd_dataa = new qint16[samples_per_record*number_of_records];
	qint16 *rd_datab = new qint16[samples_per_record*number_of_records];
	qint16 *need_rda = new qint16[samples_per_record];
	qint16 *need_rdb = new qint16[samples_per_record];
	target_buffers[0] = need_rda;
	target_buffers[1] = need_rdb;
	for(uint i = 0;i < number_of_records;i++)
	{
		QCoreApplication::processEvents();
		int rn2 = 0;
        ADQ_GetData(adq_cu,1,target_buffers,samples_per_record,
					   2,i,1,0x03,0,samples_per_record,0x00);
		for(uint j = 0;j < samples_per_record;j++)
		{
			rd_dataa[i*samples_per_record + rn2] = *(need_rda + j);
			rd_datab[i*samples_per_record + rn2] = *(need_rdb + j);
			rn2++;
		}
	}
	delete[] need_rda;
	delete[] need_rdb;
//	if(ADQ214_GetData(adq_cu,1,target_buffers,samples_per_record*number_of_records,
//				   2,0,number_of_records,0x03,0,samples_per_record,0x00) == 0)
//	{
//		delete[] rd_dataa;
//		delete[] rd_datab;
//		success_configure = false;
//		return;
//	}

	direction_angle = (mysetting.start_azAngle + num_collect*mysetting.step_azAngle)%360;
	if(plot1show)
		plotWindow_1->datashow(rd_dataa,mysetting.plsAccNum);	//绘图窗口显示cha最后一组脉冲
	if(plot2show)
		plotWindow_2->datashow(rd_datab,mysetting.plsAccNum);	//绘图窗口显示chb最后一组脉冲
	if(!threadA.isRunning())
	{
		num_running++;
		storenum->setText(QString::fromLocal8Bit("运行的存储线程数为") + QString::number(num_running));
		threadA.fileDataPara(mysetting);					//mysetting值传递给threadstore
		threadA.otherpara(timestr,direction_angle);			//组数，时间，方位角传递给threadstore
		threadA.d_memcpy(rd_dataa,rd_datab);				//采样数据传递给threadstore
		threadA.start();									//启动threadstore线程
	}
	else
		if(!threadB.isRunning())
		{
			num_running++;
			storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
			threadB.fileDataPara(mysetting);
			threadB.otherpara(timestr,direction_angle);
			threadB.d_memcpy(rd_dataa,rd_datab);
			threadB.start();
		}
		else
			if(!threadC.isRunning())
			{
				num_running++;
				storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
				threadC.fileDataPara(mysetting);
				threadC.otherpara(timestr,direction_angle);
				threadC.d_memcpy(rd_dataa,rd_datab);
				threadC.start();
			}
			else
				if(!threadD.isRunning())
				{
					num_running++;
					storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
					threadD.fileDataPara(mysetting);
					threadD.otherpara(timestr,direction_angle);
					threadD.d_memcpy(rd_dataa,rd_datab);
					threadD.start();
				}
				else	//四个线程都在运行时，停止采集
				{
					delete[] rd_dataa;
					delete[] rd_datab;
					thread_enough = false;
					return;
				}

	delete[] rd_dataa;
	delete[] rd_datab;
}

//采集数据上传和存储完成后，更新当前采集文件、序号信息
void MainWindow::update_collect_number()
{
	int filenumber = mysetting.dataFileName_Suffix.toInt();
	int len = mysetting.dataFileName_Suffix.length();
	filenumber++ ;
	if(len<=8)
	{
		mysetting.dataFileName_Suffix.sprintf("%08d", filenumber);
		mysetting.dataFileName_Suffix = mysetting.dataFileName_Suffix.right(len);
	}
	if(mysetting.isSingleCh)
		FileName_1 = mysetting.dataFileName_Prefix + "_ch1_" + mysetting.dataFileName_Suffix + ".wld";
	else
	{
		FileName_A = mysetting.dataFileName_Prefix + "_chA_" + mysetting.dataFileName_Suffix + ".wld";
		FileName_B = mysetting.dataFileName_Prefix + "_chB_" + mysetting.dataFileName_Suffix + ".wld";
	}
	num_collect++;		//下一组采集组数
}

//采集停止或结束时更新电机位置、采集卡信息
void MainWindow::collect_reset()
{
	qDebug() << "main 9collect_reset";
	stopped = true;
    ADQ_DisarmTrigger(adq_cu,1);
    ADQ_MultiRecordClose(adq_cu,1);
}

//程序关闭时，检查存储线程是否完成数据存储
void MainWindow::closeEvent(QCloseEvent *event)
{
	if((num_running != 0)||(stopped == false))
	{
		hintInfo_handle(1);
		event->ignore();
	}
	else
	{
		DeleteADQControlUnit(adq_cu);
		event->accept();
	}
}

//线程存储完成，线程数减1
void MainWindow::receive_storefinish()
{
	num_running--;
	storenum->setText(QString::fromLocal8Bit("运行的存储线程数为")+QString::number(num_running));
}
