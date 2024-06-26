#include <arrow/buffer.h>
#include <arrow/io/api.h>
#include <arrow/util/compression.h>
#include <csv_arrow_ingest.hpp>
#include <decryption.hpp>

arrow::csv::ConvertOptions
get_arrow_convert_options(const std::vector<std::string> &utf8_columns,
                          const std::string &null_value) {
  auto convert_options = arrow::csv::ConvertOptions::Defaults();
  convert_options.null_values
      .clear(); // there are a lot of default "null" values
  convert_options.null_values.push_back(null_value);
  convert_options.strings_can_be_null = true;
  // read all update-file CSV columns as text to accommodate
  // unmodified_string and null_string values
  for (auto &col_name : utf8_columns) {
    convert_options.column_types.insert({col_name, arrow::utf8()});
  }
  return convert_options;
}

template <typename T>
std::shared_ptr<arrow::Table>
read_csv_stream_to_arrow_table(T &input_stream, const IngestProperties &props) {

  auto read_options = arrow::csv::ReadOptions::Defaults();
  read_options.block_size = props.csv_block_size_mb << 20;
  auto parse_options = arrow::csv::ParseOptions::Defaults();
  parse_options.newlines_in_values = true;
  auto convert_options =
      get_arrow_convert_options(props.utf8_columns, props.null_value);

  auto maybe_table_reader = arrow::csv::TableReader::Make(
      arrow::io::default_io_context(), std::move(input_stream), read_options,
      parse_options, convert_options);

  if (!maybe_table_reader.ok()) {
    throw std::runtime_error(
        "Could not create table reader from decrypted file: " +
        maybe_table_reader.status().message());
  }
  auto table_reader = std::move(maybe_table_reader.ValueOrDie());

  auto maybe_table = table_reader->Read();
  if (!maybe_table.ok()) {
    throw std::runtime_error("Could not read CSV <" + props.filename +
                             ">: " + maybe_table.status().message());
  }
  auto table = std::move(maybe_table.ValueOrDie());

  return table;
}

std::shared_ptr<arrow::Table>
read_encrypted_csv(const IngestProperties &props) {

  std::vector<unsigned char> plaintext = decrypt_file(
      props.filename,
      reinterpret_cast<const unsigned char *>(props.decryption_key.c_str()));
  auto buffer = std::make_shared<arrow::Buffer>(
      reinterpret_cast<const uint8_t *>(plaintext.data()), plaintext.size());
  auto buffer_reader = std::make_shared<arrow::io::BufferReader>(buffer);

  arrow::Compression::type compression_type = arrow::Compression::ZSTD;
  auto maybe_codec = arrow::util::Codec::Create(compression_type);
  if (!maybe_codec.ok()) {
    throw std::runtime_error(
        "Could not create codec from ZSTD compression type: " +
        maybe_codec.status().message());
  }
  auto codec = std::move(maybe_codec.ValueOrDie());
  auto maybe_compressed_input_stream =
      arrow::io::CompressedInputStream::Make(codec.get(), buffer_reader);
  if (!maybe_compressed_input_stream.ok()) {
    throw std::runtime_error("Could not input stream from compressed buffer: " +
                             maybe_compressed_input_stream.status().message());
  }
  auto compressed_input_stream =
      std::move(maybe_compressed_input_stream.ValueOrDie());

  return read_csv_stream_to_arrow_table(compressed_input_stream, props);
}

std::shared_ptr<arrow::Table>
read_unencrypted_csv(const IngestProperties &props) {

  auto maybe_file = arrow::io::ReadableFile::Open(props.filename,
                                                  arrow::default_memory_pool());
  if (!maybe_file.ok()) {
    throw std::runtime_error("Could not open uncompressed file <" +
                             props.filename +
                             ">: " + maybe_file.status().message());
  }
  auto plaintext_input_stream = std::move(maybe_file.ValueOrDie());

  return read_csv_stream_to_arrow_table(plaintext_input_stream, props);
}
