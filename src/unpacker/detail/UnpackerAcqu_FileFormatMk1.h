#ifndef UNPACKERACQU_FILEFORMATMK1_H
#define UNPACKERACQU_FILEFORMATMK1_H

#include "UnpackerAcqu_detail.h"

namespace ant {
namespace unpacker {
namespace acqu {

class FileFormatMk1 : public FileFormatBase {

  // UnpackerAcquFile interface
protected:
  virtual size_t SizeOfHeader() const override;
  virtual bool InspectHeader(const std::vector<std::uint32_t>& buffer) const override;
  virtual void FillInfo(reader_t& reader, buffer_t& buffer, Info& info) const override;
  virtual void FillFirstDataBuffer(queue_t& queue, reader_t& reader, buffer_t& buffer) const override;
  virtual bool UnpackDataBuffer(queue_t &queue, it_t& it, const it_t& it_endbuffer) noexcept override;

};

}}} // namespace ant::unpacker::acqu


#endif // UNPACKERACQU_FILEFORMATMK1_H
