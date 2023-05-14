//
//  main.cpp
//  VideoDecoder
//
//  Created by sjpark on 2023/05/12.
//
#include "VideoDecoder.h"

int main(int argc, char* argv[]) {
    
    //FindIFrame(argc, argv);
    //NaiveDecoding(argc, argv);
    FastDecoding(argc, argv);
	//FastDecoding2(argc, argv);
    return 0;
    
}
