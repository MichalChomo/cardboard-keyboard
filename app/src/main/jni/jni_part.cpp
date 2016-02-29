#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include "aruco.h"
#include <vector>

using namespace std;
using namespace cv;
using namespace aruco;

extern "C" {

JNIEXPORT void JNICALL
Java_cz_email_michalchomo_cardboardkeyboard_MainActivity_FindFeatures(JNIEnv *env, jobject instance,
                                                                      jlong matAddrGr,
                                                                      jlong matAddrRgba) {

    Mat &mGr = *(Mat *) matAddrGr;
    Mat &mRgb = *(Mat *) matAddrRgba;
    //vector<KeyPoint> v;
    MarkerDetector markerDetector;
    vector<Marker> markers;

    markerDetector.detect(mGr, markers);
    for (unsigned int i=0; i < markers.size(); i++) {
        cout << markers[i] << endl;
        markers[i].draw(mRgb,Scalar(0,0,255),2);
    }

    /*Ptr<FeatureDetector> detector = FastFeatureDetector::create(50);
    detector->detect(mGr, v);
    for (unsigned int i = 0; i < v.size(); i++) {
        const KeyPoint &kp = v[i];
        circle(mRgb, Point(kp.pt.x, kp.pt.y), 10, Scalar(255, 0, 0, 255));
    }*/

}
}