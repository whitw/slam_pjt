// compile option
// g++ -std=c++14 -O2 ./send.cpp -o ./send -L/usr/local/include/opencv2/ -lopencv_videoio -lopencv_core -lopencv_imgcodecs -lpaho-mqttpp3 -lpaho-mqtt3as

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio/videoio_c.h>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <chrono>

#include <nlohmann/json.hpp>
#include <mqtt/client.h>

using namespace std;
using namespace cv;
using json = nlohmann::json;

int main()
{
    ifstream configFile("config.txt");
    string line;
    string client_address;
    string client_id;
    while(getline(configFile, line)) {
    	if (line.substr(0, 11) == "IP_ADDRESS=") {
		client_address = line.substr(11);
	}
	if (line.substr(0, 10) == "CLIENT_ID=") {
		client_id = line.substr(10);
	}
    }

    mqtt::async_client cli(client_address, client_id);
    cli.connect()->wait();
    mqtt::topic topic(cli, "kmg_img");

    // send the images
    VideoCapture cap("/dev/video0");
    if (!cap.isOpened())
    {
        cerr << "Failed to open video file." << endl;
        return EXIT_FAILURE;
    }

    system("v4l2-ctl -c brightness=85"); // 0 ~ 100, default = 50
    system("v4l2-ctl -c contrast=80");   // -100 ~ 100, default = 0
    system("v4l2-ctl -c saturation=80"); // -100 ~ 100, default = 0
    system("v4l2-ctl -c sharpness=100"); // 예리함 정도
    system("v4l2-ctl -c rotate=180");    // 회전

    system("v4l2-ctl -c auto_exposure=0");           // 0:auto, 1:manual
    system("v4l2-ctl -c exposure_time_absolute=30"); // 1 ~ 10000, default = 1000
    system("v4l2-ctl -c auto_exposure_bias=7");      // 노출 기준값

    system("v4l2-ctl -c image_stabilization=1");    // 흔들림 방지
    system("v4l2-ctl -c iso_sensitivity=4");        // 0 ~ 4, default = 0
    system("v4l2-ctl -c iso_sensitivity_auto=1");   // 0:manual, 1:auto
    system("v4l2-ctl -c exposure_metering_mode=0"); // 0:average, 1:center, 2:spot, 3:matrix

    system("v4l2-ctl -p 20"); // fps

    // const auto t0 = chrono::high_resolution_clock::now();
    //  for(int i = 0; i < 20; ++i)
    bool flag = 0;
    for (;;)
    {
        Mat frame;
        cap.read(frame);
        const auto tframe = chrono::high_resolution_clock::now().time_since_epoch().count();

        struct __attribute__((__packed__)) HEADER
        {
            int bufsz;
            uint64_t stamp;
        } tmp;

        // const auto tframe = chrono::duration_cast<chrono::nanoseconds>(t - t0).count();
        // cout << tframe << " ms\n";
        if (frame.empty())
        {
            cerr << "ERROR! blank frame grabbed\n";
            break;
        }

        vector<uchar> buffer;
        buffer.reserve(200000);
        imencode(".jpg", frame, buffer);
        tmp.bufsz = buffer.size();
        tmp.stamp = tframe;
        buffer.insert(buffer.begin(), (uchar *)(&tmp), (uchar *)(&tmp) + sizeof(tmp));

        mqtt::token_ptr tok = topic.publish(string(buffer.begin(), buffer.end()));
        tok->wait();
        
        cout << buffer.size() << "\n";
    }

    cli.disconnect();
    cap.release();

    return EXIT_SUCCESS;
}
