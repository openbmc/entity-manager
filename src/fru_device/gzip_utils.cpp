#include "gzip_utils.hpp"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <unistd.h>
#include <zlib.h>

#include <optional>
#include <span>
#include <string>
#include <vector>

std::optional<std::string> gzipInflate(std::span<const uint8_t> compressedBytes)
{
    std::string uncompressedBytes;
    if (compressedBytes.empty())
    {
        return std::nullopt;
    }

    z_stream strm{};
    strm.next_in = std::bit_cast<Bytef*>(compressedBytes.data());
    strm.avail_in = static_cast<uInt>(compressedBytes.size());
    strm.total_out = 0;

    if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK)
    {
        return std::nullopt;
    }

    while (strm.avail_in > 0)
    {
        constexpr size_t chunkSize = 1024;
        uncompressedBytes.resize(uncompressedBytes.size() + chunkSize);
        strm.next_out =
            std::bit_cast<Bytef*>(uncompressedBytes.end() - chunkSize);
        strm.avail_out = chunkSize;

        // Inflate another chunk.
        int err = inflate(&strm, Z_SYNC_FLUSH);
        if (err == Z_STREAM_END)
        {
            break;
        }
        if (err != Z_OK)
        {
            return std::nullopt;
        }
    }

    if (inflateEnd(&strm) != Z_OK)
    {
        return std::nullopt;
    }
    uncompressedBytes.resize(strm.total_out);

    return {uncompressedBytes};
}

static std::vector<std::string> xpathText(xmlDocPtr doc, const char* xp)
{
    std::vector<std::string> val;
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (ctx == nullptr)
    {
        return val;
    }
    const unsigned char* xpptr = std::bit_cast<const unsigned char*>(xp);
    xmlXPathObjectPtr obj = xmlXPathEvalExpression(xpptr, ctx);
    if (obj != nullptr)
    {
        if (obj->type == XPATH_NODESET && obj->nodesetval != nullptr)
        {
            xmlNodeSetPtr nodeTab = obj->nodesetval;
            size_t nodeNr = static_cast<size_t>(nodeTab->nodeNr);
            std::span<xmlNodePtr> nodes{nodeTab->nodeTab, nodeNr};
            for (xmlNodePtr node : nodes)
            {
                unsigned char* keyword = xmlNodeGetContent(node);
                val.emplace_back(std::bit_cast<const char*>(keyword));
                xmlFree(keyword);
            }
        }
    }

    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
    return val;
}

std::vector<std::string> getNodeFromXml(std::string_view xml,
                                        const char* nodeName)
{
    std::vector<std::string> node;
    if (xml.empty())
    {
        return node;
    }
    xmlDocPtr doc = xmlReadMemory(
        xml.data(), xml.size(), nullptr, nullptr,
        XML_PARSE_RECOVER | XML_PARSE_NONET | XML_PARSE_NOWARNING);
    if (doc == nullptr)
    {
        return {};
    }
    node = xpathText(doc, nodeName);
    xmlFreeDoc(doc);
    return node;
}
