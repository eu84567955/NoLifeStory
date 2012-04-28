///////////////////////////////////
// Copyright 2012 Peter Atechian //
// Licensed under GPLv3          //
///////////////////////////////////

#include "../wz/wz.h"
#include <iostream>
#include <ctime>
#include <thread>
using namespace std;

int main() {
    clock_t start = clock();
    WZ::Paths.push_back("");
    //WZ::Paths.push_back("C:/Nexon/MapleStory 40b/");
    WZ::Paths.push_back("C:/Nexon/MapleStory 109/");
	WZ::Paths.push_back("C:/Nexon/MapleStory/");
    WZ::Paths.push_back("D:/Program Files (x86)/MapleStory_gms_109/");
    WZ::Init();
    clock_t end = clock();
    cout << "Time taken: " << end - start << "ms" << endl;
    cin.get();
}