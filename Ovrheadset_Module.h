#pragma once
#include <yarp/os/all.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Network.h>
#include <yarp/os/Thread.h>
#include <yarp/dev/all.h>
#include <yarp/sig/all.h>
#include <yarp/dev/IJoypadController.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::sig;

class MyModule :public RFModule
{

	string robotName;
	string inputPort;
	string localPort;
	string inputPortL;
	string inputPortR;
	string localPortL;
	string localPortR;
	ImageOf<PixelRgb> imgL;
	ImageOf<PixelRgb> imgR;
	PolyDriver OvrHeadset;
	PolyDriver camL;
	PolyDriver camR;
	IFrameGrabberImage *IframR;
	IFrameGrabberImage *IframL;
	IJoypadController * IJoyPad;

	BufferedPort<yarp::sig::ImageOf<PixelRgb>> imageIn;      
	BufferedPort<yarp::sig::ImageOf<PixelRgb>> imageOut;

public:

	bool configure(yarp::os::ResourceFinder &rf); 
	bool interruptModule();                       
	bool close();                                
	bool respond(const Bottle& command, Bottle& reply);
	double getPeriod();
	bool updateModule();
};
