#include "stxxl_writer.h"

STXXLEdgeWriter *stxxl_writer_open_file(const char *filename, long long int elements_estimated) {
    return new STXXLEdgeWriter(filename, elements_estimated);
}

void stxxl_writer_push(STXXLEdgeWriter *writer, const STXXLEdge *edge) {
    writer->writer << *edge;
    writer->edges_written++;
}

void stxxl_writer_close(STXXLEdgeWriter *writer) {
    writer->writer.finish();
    writer->vector.resize(writer->edges_written, true);
    delete writer;
}

long long int stxxl_writer_edges(STXXLEdgeWriter *writer) {
    return writer->edges_written;
}