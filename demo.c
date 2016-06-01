#include <stdio.h>
#include "stxxl_writer.h"

int main(int argc, char** argv) {
    size_t nodes = 1000;

    // Open the file. If you already know how many edges there will be,
    // you may provide a lower bound (this amount will be reserved in the file system)
    // If you dont know it - no harm done, the writer will take care of it
    // but may be slightly(!) slower
    // The function will allocate the required memory
    STXXLEdgeWriter* writer = stxxl_writer_open_file("clique.bin", 1 << 20);

    STXXLEdge edge;
    edge.first = nodes-1;
    edge.second = 0;

    for(edge.first = 1; edge.first < nodes; edge.first++) {
        for(edge.second = 0; edge.second < edge.first; edge.second++) {
            // just push every edge
            stxxl_writer_push(writer, &edge);
        }
    }

    printf("About to close writer; got %ld edges\n", stxxl_writer_edges(writer));

    // And don't forget close the file; this is really important, since
    // we are using buffers and asynchronous writes, which may not happen
    // if you don't close it. This function also frees up any memory allocated
    // earlier
    stxxl_writer_close(writer);

    return 0;
}