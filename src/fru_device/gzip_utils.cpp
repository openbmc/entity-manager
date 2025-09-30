#include "gzip_utils.hpp"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <unistd.h>
#include <zlib.h>

#include <optional>
#include <span>
#include <string>

std::optional<std::string> gzipInflate(std::span<uint8_t> compressedBytes)
{
    std::string uncompressedBytes;
    if (compressedBytes.empty())
    {
        return std::nullopt;
    }

    z_stream strm{

    };
    strm.next_in = (Bytef*)compressedBytes.data();
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

static std::string xpathText(xmlDocPtr doc, const char* xp)
{
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (ctx == nullptr)
    {
        return {};
    }
    xmlXPathObjectPtr obj =
        xmlXPathEvalExpression(std::bit_cast<const unsigned char*>(xp), ctx);
    std::string val;
    if ((obj != nullptr) && (obj->nodesetval != nullptr) &&
        obj->nodesetval->nodeNr > 0)
    {
        xmlNodePtr n = *obj->nodesetval->nodeTab;
        if ((n != nullptr) && (n->children != nullptr) &&
            (n->children->content != nullptr))
        {
            val = std::bit_cast<const char*>(n->children->content);
        }
    }
    if (obj != nullptr)
    {
        xmlXPathFreeObject(obj);
    }
    xmlXPathFreeContext(ctx);
    return val;
}

std::string getNodeFromXml(const std::string& xml, const char* nodeName)
{
    if (xml.empty())
    {
        return {};
    }
    xmlDocPtr doc = xmlReadMemory(xml.data(), xml.size(), nullptr, nullptr,
                                  XML_PARSE_RECOVER);
    if (doc == nullptr)
    {
        return {};
    }
    std::string node = xpathText(doc, nodeName);
    xmlFreeDoc(doc);
    return node;
}
