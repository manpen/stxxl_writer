#ifndef _STXXL_WRITER_HEADER
#define _STXXL_WRITER_HEADER

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

// You may alter this type
typedef struct STXXLEdge STXXLEdge;
struct STXXLEdge {
    long long unsigned int first;
    long long unsigned int second;

#ifdef __cplusplus
    bool operator==(const STXXLEdge& r) const {
        return first==r.first && second==r.second;
    }
#endif
};

#ifdef __cplusplus
    #include <stxxl/io>
    #include <stxxl/vector>

    struct STXXLEdgeWriter {
       typedef stxxl::VECTOR_GENERATOR<STXXLEdge>::result vector_type;
       typedef vector_type::bufwriter_type bufwriter_type;

       stxxl::linuxaio_file file;
       vector_type vector;
       bufwriter_type writer;
       long long int edges_written;

       STXXLEdgeWriter(const std::string & filename, uint64_t expected_num_elems = 0)
             : file(filename, stxxl::file::DIRECT | stxxl::file::RDWR | stxxl::file::CREAT | stxxl::file::TRUNC)
             , vector(&file)
             , writer(vector.begin())
             , edges_written(0)
       {
          if (expected_num_elems) {
             vector.resize(expected_num_elems);
          }
       }
    };
#else
    typedef struct STXXLEdgeWriter STXXLEdgeWriter;
#endif

#ifdef __cplusplus
extern "C" {
#endif


    STXXLEdgeWriter* stxxl_writer_open_file(const char* filename, long long int elements_estimated);
    void stxxl_writer_push(STXXLEdgeWriter* obj, const STXXLEdge* edge);
    void stxxl_writer_close(STXXLEdgeWriter* obj);
    long long int stxxl_writer_edges(STXXLEdgeWriter *writer);


#ifdef __cplusplus
}
#endif

#endif // _STXXL_WRITER_HEADER