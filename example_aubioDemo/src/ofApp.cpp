//Adapted by Amanda Lind Fall 2016
#include "ofApp.h"
#include "ofEventUtils.h"
#include "asio.hpp" //For socket integration
#include "json.hpp" //For json socket integration
#include <string.h>
#include "oscpkt.hh"


//---------------------Global Var: JSON------------------------------------

using json = nlohmann::json;
json aubio_output_json;

//---------------------Global Var: ASIO------------------------------------

std::string s;
using asio::ip::udp;
asio::io_service io_service;
#define PORT "1330"

//---------------------Global Var: OSC-------------------------------------

using namespace oscpkt;
PacketWriter pkt;
Message msg;
const void * message;
int size;

//---------------------UDP Classes-----------------------------------------

class UDPClient
{
public:
    UDPClient(
              asio::io_service& io_service,
              const std::string& host,
              const std::string& port
              ) : io_service_(io_service), socket_(io_service, udp::endpoint(udp::v4(), 0)) {
        udp::resolver resolver(io_service_);
        udp::resolver::query query(udp::v4(), host, port);
        udp::resolver::iterator iter = resolver.resolve(query);
        endpoint_ = *iter;
    }
    
    ~UDPClient()
    {
        socket_.close();
    }
    
    void send(const std::string& msg) {
        socket_.send_to(asio::buffer(msg, msg.size()), endpoint_);
    }
    
    void send_osc(const void *msg, int size) {
        socket_.send_to(asio::buffer(msg, size), endpoint_);
    }
    
private:
    asio::io_service& io_service_;
    udp::socket socket_;
    udp::endpoint endpoint_;
};


UDPClient client(io_service, "localhost", PORT);


//---------------------OF/AUBIO Classes-----------------------------------------
void ofApp::setup(){
  
    
    // set the size of the window
    ofSetWindowShape(750, 250);

    int nOutputs = 2;
    int nInputs = 2;
    //int sampleRate = 44100;
    //int bufferSize = 256;
    //int nBuffers = 4;

    // setup onset object
    onset.setup();
    //onset.setup("mkl", 2 * bufferSize, bufferSize, sampleRate);
    // listen to onset event
    ofAddListener(onset.gotOnset, this, &ofApp::onsetEvent);

    // setup pitch object
    pitch.setup();
    //pitch.setup("yinfft", 8 * bufferSize, bufferSize, sampleRate);

    // setup beat object
    beat.setup();
    //beat.setup("default", 2 * bufferSize, bufferSize, samplerate);
    // listen to beat event
    ofAddListener(beat.gotBeat, this, &ofApp::beatEvent);

    // setup mel bands object
    bands.setup();

    ofSoundStreamSetup(nOutputs, nInputs, this);
    //ofSoundStreamSetup(nOutputs, nInputs, sampleRate, bufferSize, nBuffers);
    //ofSoundStreamListDevices();

    // setup the gui objects
    int start = 0;
    beatGui.setup("ofxAubioBeat", "settings.xml", start + 10, 10);
    beatGui.add(bpm.setup( "bpm", 0, 0, 250));

    start += 250;
    onsetGui.setup("ofxAubioOnset", "settings.xml", start + 10, 10);
    onsetGui.add(onsetThreshold.setup( "threshold", 0, 0, 2));
    onsetGui.add(onsetNovelty.setup( "onset novelty", 0, 0, 10000));
    onsetGui.add(onsetThresholdedNovelty.setup( "thr. novelty", 0, -1000, 1000));
    // set default value
    onsetThreshold = onset.threshold;

    start += 250;
    pitchGui.setup("ofxAubioPitch", "settings.xml", start + 10, 10);
    pitchGui.add(midiPitch.setup( "midi pitch", 0, 0, 128));
    pitchGui.add(pitchConfidence.setup( "confidence", 0, 0, 1));

    bandsGui.setup("ofxAubioMelBands", "settings.xml", start + 10, 115);
    for (int i = 0; i < 40; i++) {
        bandPlot.addVertex( 50 + i * 650 / 40., 240 - 100 * bands.energies[i]);
    }
    
}

void ofApp::exit(){
    ofSoundStreamStop();
    ofSoundStreamClose();
}

void ofApp::audioIn(float * input, int bufferSize, int nChannels){
    // compute onset detection
    onset.audioIn(input, bufferSize, nChannels);
    // compute pitch detection
    pitch.audioIn(input, bufferSize, nChannels);
    // compute beat location
    beat.audioIn(input, bufferSize, nChannels);
    // compute bands
    bands.audioIn(input, bufferSize, nChannels);
}

void audioOut(){
}

//--------------------------------------------------------------
void ofApp::update(){
    onset.setThreshold(onsetThreshold);
}

//--------------------------------------------------------------
void ofApp::draw(){


    
    // update beat info
    if (gotBeat) {
        ofSetColor(ofColor::green);
        ofRect(90,150,50,50);
        gotBeat = false;
    }

    // update onset info
    if (gotOnset) {
        ofSetColor(ofColor::red);
        ofRect(250 + 90,150,50,50);
        gotOnset = false;
    }
    onsetNovelty = onset.novelty;
    onsetThresholdedNovelty = onset.thresholdedNovelty;
    std::cout<<"Onset Threshold Novelty:"<<onsetThresholdedNovelty<<endl ;

    // update pitch info
    pitchConfidence = pitch.pitchConfidence;
    if (pitch.latestPitch) midiPitch = pitch.latestPitch;
    std::cout<<"Pitch:"<<midiPitch<<endl ;

    // update BPM
    bpm = beat.bpm;
    std::cout<<"BPM:"<<bpm<<endl ;

    // Make JSON Object
    aubio_output_json["onsetThresholdedNovelty"] = onset.thresholdedNovelty;
    aubio_output_json["midiPitch"] = pitch.latestPitch;
    aubio_output_json["bpm"] = beat.bpm;
    std::string s = aubio_output_json.dump();
    //client.send("Test");      //This is a string
    //client.send(s);           //This is JSON
    
    // Make OSC Object
    pkt.startBundle();
    pkt.addMessage(msg.init("/aubio/onset").pushFloat(onset.thresholdedNovelty));
    pkt.addMessage(msg.init("/aubio/midiPitch").pushFloat(pitch.latestPitch));
    pkt.addMessage(msg.init("/aubio/bpm").pushFloat(beat.bpm));
    pkt.endBundle();
    if (pkt.isOk()) {
        message=pkt.packetData();
        size= pkt.packetSize();
        client.send_osc(message, size);
    }
    msg.clear();
    pkt.Reset();
    
    // draw
    
    pitchGui.draw();
    beatGui.draw();
    onsetGui.draw();

    ofSetColor(ofColor::orange);
    ofSetLineWidth(3.);
    bandsGui.draw();
    //bandPlot.clear();
    for (int i = 0; i < bandPlot.size(); i++) {
        bandPlot[i].y = 240 - 100 * bands.energies[i];
    }
    bandPlot.draw();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}

//----
void ofApp::onsetEvent(float & time) {
    //ofLog() << "got onset at " << time << " s";
    gotOnset = true;
}

//----
void ofApp::beatEvent(float & time) {
    //ofLog() << "got beat at " << time << " s";
    gotBeat = true;
}



