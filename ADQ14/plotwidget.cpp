#include "plotwidget.h"

#include <qwt_counter.h>
#include <qwt_picker_machine.h>
#include <qwt_plot_zoomer.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_renderer.h>
#include <qwt_text.h>
#include <qwt_math.h>
#include <QMessageBox>

#include "qwt_plot_grid.h"
#include "qwt_plot_curve.h"
#include "mainwindow.h"

class Zoomer : public QwtPlotZoomer
{
public:
	Zoomer(int xAxis,int yAxis,QWidget *canvas):
		QwtPlotZoomer(xAxis,yAxis,canvas)
	{
		setTrackerMode(QwtPicker::AlwaysOff);
		setRubberBand(QwtPicker::NoRubberBand);

		setMousePattern(QwtEventPattern::MouseSelect2,
						Qt::RightButton,Qt::ControlModifier);
		setMousePattern(QwtEventPattern::MouseSelect3,
						Qt::RightButton);
	}
};

PlotWindow::PlotWindow(MainWindow *parent):QDialog(parent)
{
	qwtPlot = new QwtPlot(this);
	//����������
	qwtPlot->setAxisScale(qwtPlot->xBottom,0,1000);					//x��ֵ�ĳ�ʼ��Χ0��1000
	qwtPlot->setAxisScale(qwtPlot->yLeft,0,5);						//y��ֵ�ĳ�ʼ��Χ0��5
	//��������
	grid = new QwtPlotGrid;
	grid->enableX(true);
	grid->enableY(true);
	grid->enableXMin(false);
	grid->enableYMin(false);
	grid->setMajorPen(QPen(Qt::black,0,Qt::DotLine));				//�����ɫ
	grid->attach(qwtPlot);
	//�������Ź���
	d_zoomer = new Zoomer(QwtPlot::xBottom,QwtPlot::yLeft,
						  qwtPlot->canvas());
	d_zoomer->setRubberBand(QwtPicker::RectRubberBand);
	d_zoomer->setRubberBandPen(QColor(Qt::blue));					//���ѡ�п�Ϊ��ɫ
	d_zoomer->setTrackerMode(QwtPicker::ActiveOnly);
	d_zoomer->setTrackerPen(QColor(Qt::blue));						//���ѡ������Ϊ��ɫ
	//�������ƹ���
	d_panner = new QwtPlotPanner(qwtPlot->canvas());
	d_panner->setMouseButton(Qt::MidButton);						//����м们����������

	//	//����ͨ����
	//	qwtPlot->setTitle(QStringLiteral("ch")+ QString::number(i));

	parentWindow = parent;
	enableZoomMode(true);											//�Ŵ�ƽ�ƹ���

    xValues = nullptr;
    yValues = nullptr;

	H_trim = 20;													//������ʾʱ���ױ߿�����ף�ȫ��ʱΪ60��
	//��������
	qwtPlotCurve = new QwtPlotCurve;
	qwtPlotCurve->setStyle(QwtPlotCurve::Steps);
	qwtPlotCurve->setRenderHint(QwtPlotCurve::RenderAntialiased);
	qwtPlotCurve->attach(qwtPlot);
}

void PlotWindow::show()
{
	const int W_trim = 20;											//�Ҳ�������20
	qwtPlot->setFixedSize(width()-W_trim,height()-H_trim);			//�̶��ߴ�
	QWidget::show();
	//	qwtPlot->setMinimumSize(width()-W_trim,height()-H_trim);		//��ͼ������С�ߴ�
}

void PlotWindow::enableZoomMode(bool on)							//���÷Ŵ�ƽ�ƹ���
{
	d_panner->setEnabled(on);

	d_zoomer->setEnabled(on);
	d_zoomer->zoom(0);
}

void PlotWindow::setMaxX(int xnum,int s_freq,bool count_num)
{
	plot_sampleNum = xnum;											//��������
	plot_sampleFreq = s_freq;										//����Ƶ��

    if(xValues != nullptr || yValues != nullptr)							//x,y����
	{
		delete [] xValues;
		delete [] yValues;
        xValues = nullptr;
        yValues = nullptr;
	}
	xValues = new double[plot_sampleNum];							//x,y����ĵ���
	yValues = new double[plot_sampleNum];

	for(uint i = 0; i<plot_sampleNum; i++)							//������Ĭ����������0
		yValues[i] = 0;
	update_xAxis(count_num);										//���º�������ʾ����
}

//��ͼ������ʾ
void PlotWindow::datashow(const qint16 *datas,uint pnum)
{
	for(uint i = 0;i < plot_sampleNum;i++)
		yValues[i] = *(datas + (plot_sampleNum*(pnum-1)+i));
	if(d_zoomer->zoomRectIndex() == 0)
	{
		qwtPlotCurve->setSamples(xValues,yValues,plot_sampleNum);
		d_zoomer->setZoomBase(true);
	}
	qwtPlot->replot();
}

//���º�������ʾ����
void PlotWindow::update_xAxis(bool countnum)
{
	QFont axistitlefont;
	axistitlefont.setFamily("Microsoft YaHei UI");
	axistitlefont.setPixelSize(15);
	axistitlefont.setBold(false);
	QwtText axistitle1(QString::fromLocal8Bit("��������"));
	QwtText axistitle2(QString::fromLocal8Bit("���루m��"));
    axistitle1.setFont(axistitlefont);
	axistitle2.setFont(axistitlefont);
	if(countnum)
	{
		qwtPlot->setAxisScale(qwtPlot->xBottom,0,plot_sampleNum);	//����x�᷶Χ
		qwtPlot->setAxisTitle(QwtPlot::xBottom,axistitle1);
		for(uint i = 0; i<plot_sampleNum; i++)						//x���г�ʼ��ֵ
			xValues[i] = i;											//������Ϊ�������
	}
	else
	{
		qwtPlot->setAxisScale(qwtPlot->xBottom,0,plot_sampleNum*150/plot_sampleFreq);
		qwtPlot->setAxisTitle(QwtPlot::xBottom,axistitle2);
		for(uint i = 0; i<plot_sampleNum; i++)						//x���г�ʼ��ֵ
			xValues[i] = (float)(i)*150/plot_sampleFreq;			//������xת���ɳ��ȵ�λ
	}
	qwtPlot->setAxisAutoScale(qwtPlot->yLeft,true);					//y���Զ�����
	qwtPlotCurve->setSamples(xValues,yValues,plot_sampleNum);
	d_zoomer->setZoomBase(true);									//ԭʼ�����������Χ����
	qwtPlot->replot();
}

//���ø�ͨ����
void PlotWindow::set_titleName(QString ch_name)
{
	QwtText title(ch_name);
	QFont titlefont;
	titlefont.setFamily("Microsoft YaHei UI");
	titlefont.setPixelSize(15);
	titlefont.setBold(false);
	title.setFont(titlefont);
	qwtPlot->setTitle(title);
}

void PlotWindow::set_grid(bool hidegrid)
{
	if(hidegrid)
	{
		grid->enableX(false);
		grid->enableY(false);
	}
	else
	{
		grid->enableX(true);
		grid->enableY(true);
	}
	qwtPlot->replot();
}

void PlotWindow::keyPressEvent(QKeyEvent *)                         //�����ڼ��̰����¼�
{
}

void PlotWindow::timerEvent(QTimerEvent *)
{
}

void PlotWindow::resizeEvent(QResizeEvent *)						//��ͼ���ڴ�С�ı�ʱ
{
	if(height()>=1060)
		H_trim = 60;
	else
		H_trim = 20;
	show();
}
