
#include <mysql/psi/mysql_file.h>
#include <mysys_priv.h>
class RingBuffer
{
public:
  RingBuffer(File file, size_t cachesize);
  int read(uchar *To, size_t Count);
  int write(uchar *From, size_t Count);
  ~RingBuffer();

private:
  File _file;

  /* buffer writes */
  uchar *_write_buffer;
  /* Points to the current read position in the write buffer. */
  uchar *_append_read_pos;

  /* Points to current write position in the write buffer */
  uchar *_write_pos;

  /* The non-inclusive boundary of the valid write area */
  uchar *_write_end;

  /* Offset in file corresponding to the first byte of uchar* buffer. */
  my_off_t _pos_in_file;
  /*
    Maximum of the actual end of file and
    the position represented by read_end.
  */
  my_off_t _end_of_file;
  /* Points to current read position in the buffer */
  uchar *_read_pos;
  /* the non-inclusive boundary in the buffer for the currently valid read */
  uchar *_read_end;

  /* read buffer */
  uchar *_buffer;

  int _seek_not_done;

  size_t _alloced_buffer;

  size_t _buffer_length;

  uchar *_write_new_pos;

  mysql_mutex_t _buffer_lock;

  /* For a synchronized writer. */
  mysql_cond_t _cond_writer;

  /* To sync on writers into buffer. */
  mysql_mutex_t _mutex_writer;
};

int RingBuffer::write(uchar *From, size_t Count) { return 0; }
int RingBuffer::read(uchar *To, size_t Count) { return 0; }
RingBuffer::RingBuffer(File file, size_t cachesize) : _file(file)
{

  if (_file >= 0)
  {
    my_off_t pos;
    pos= mysql_file_tell(file, MYF(0));
    assert(pos != (my_off_t) -1);
  }

  size_t min_cache=IO_SIZE*2;

  // Calculate end of file to avoid allocating oversized buffers
  _end_of_file= mysql_file_seek(file, 0L, MY_SEEK_END, MYF(0));
  // Need to reset seek_not_done now that we just did a seek.
  _seek_not_done= 0;


  // Retry allocating memory in smaller blocks until we get one
  cachesize= ((cachesize + min_cache-1) & ~(min_cache-1));
  for (;;)
  {
    size_t buffer_block;

    if (cachesize < min_cache)
      cachesize = min_cache;
    buffer_block= cachesize * 2;

    if ((_buffer= (uchar*) my_malloc(key_memory_IO_CACHE, buffer_block, (myf) MY_WME)) != 0)
    {
      _write_buffer= _buffer + cachesize;
      _alloced_buffer= buffer_block;
      break;					// Enough memory found
    }
    assert(cachesize != min_cache); // Can't alloc cache
    // Try with less memory
    cachesize= (cachesize*3/4 & ~(min_cache-1));
  }

  _buffer_length = cachesize;
  _read_pos = _buffer;
  _append_read_pos = _write_pos = _write_buffer;
  _write_end = _write_buffer + _buffer_length;

  _read_end = _buffer;

  _write_new_pos = _write_pos;
  mysql_mutex_init(key_IO_CACHE_append_buffer_lock,
                   &_buffer_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_IO_CACHE_SHARE_cond_writer, &_cond_writer, 0);
  mysql_mutex_init(key_IO_CACHE_SHARE_mutex, &_mutex_writer, MY_MUTEX_INIT_FAST);
}
RingBuffer::~RingBuffer() {
  if (_file != -1) /* File doesn't exist */
  {
    //my_b_flush_io_cache(info, 1);
  }
  my_free(_buffer);
  mysql_mutex_destroy(&_buffer_lock);
  mysql_cond_destroy(&_cond_writer);
  mysql_mutex_destroy(&_mutex_writer);
}
