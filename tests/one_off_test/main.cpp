#include "stxxl/vector"
#include "stxxl/io"

#include <iostream>
#include <cstdint>

struct entry_64
{
  uint8_t data[32];
  uint64_t field1;
  uint64_t field2;
  uint64_t field3;
  uint64_t field4;
};

struct entry_72
{
  uint8_t data[32];
  uint64_t field1;
  uint64_t field_extra;
  uint64_t field2;
  uint64_t field3;
  uint64_t field4;
};

struct entry_128
{
  uint8_t data[32];
  uint64_t field1;
  uint64_t field_extra;
  uint64_t field2;
  uint64_t field3;
  uint64_t field4;
  uint8_t padding[56];
};

template <typename T>
void fill_entry(T& e) {
  memset(e.data, 0x66, sizeof(e.data));
  e.field1 = UINT64_C(0x1234567890123456);
  e.field2 = e.field1;
  e.field3 = e.field1;
  e.field4 = e.field1;
}

template <typename T>
void check_entry(T& e)
{
  for (int j=0; j < 32; j++) {
    if (e.data[j] != 0x66) {
      std::cout << "not = to 0x66" << std::endl;
    }
  }
}

int main(int argc, char *argv[])
{
  typedef stxxl::VECTOR_GENERATOR<entry_64>::result vec_64;
  typedef stxxl::VECTOR_GENERATOR<entry_72, 4, 8, 2 * 1024 * 1024 * 72 / 64>::result vec_72;
  typedef stxxl::VECTOR_GENERATOR<entry_128>::result vec_128;
  
  stxxl::config::get_instance()->add_disk(stxxl::disk_config("memory_no_use", 1, "memory autogrow"));

  std::cout << "checking vec_64" << std::endl;
  {
    auto *f1 = new stxxl::syscall_file("vec_64.tmp", stxxl::file::RDWR | stxxl::file::CREAT | stxxl::file::TRUNC);
    vec_64 the_vec(f1);
    
    entry_64 e1;
    fill_entry(e1);
    for (int i=0; i < 128; i++) {
      the_vec.push_back(e1);
    }
    the_vec.flush();
    
    for (int i=0; i < 128; i++) {
      check_entry(the_vec[i]);
    }
  }

  std::cout << "checking vec_72" << std::endl;
  {
    auto *f1 = new stxxl::syscall_file("vec_72.tmp", stxxl::file::RDWR | stxxl::file::CREAT | stxxl::file::TRUNC);
    vec_72 the_vec(f1);
    
    entry_72 e1;
    fill_entry(e1);
    e1.field_extra = UINT64_C(0x1111111111111111);
    for (int i=0; i < 128; i++) {
      the_vec.push_back(e1);
    }
    the_vec.flush();
    
    for (int i=0; i < 128; i++) {
      check_entry(the_vec[i]);
    }
  }
  
  std::cout << "done" << std::endl;
  
  return 0;
}



