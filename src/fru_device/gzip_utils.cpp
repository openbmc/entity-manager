#include "gzip_utils.hpp"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <unistd.h>
#include <zlib.h>

#include <span>

// Decompress gzip data from memory using zlib. Returns empty string on error.
// compressed string here should be const, but unfortunately
// zlib in our build isn't built with ZLIB_CONST, and enabling it
// is a huge blast radius. Because of this,
// the next_in variable inside of z_stream isn't decorated as const
// so marking compressed as const requires a bunch of const_cast hackery.
// marking it as const is therefore not much of a guarantee.
std::string decompressGzip(std::span<uint8_t> compressed)
{
    if (compressed.empty())
    {
        return {};
    }

    z_stream stream{};
    int rc = inflateInit2(&stream, 16 + MAX_WBITS); // enable gzip decoding
    if (rc != Z_OK)
    {
        return {};
    }

    std::string output;
    std::array<unsigned char, 8192> outbuf{};

    size_t inPos = 0;
    int ret = Z_OK;
    while (true)
    {
        if (stream.avail_in == 0 && inPos < compressed.size())
        {
            size_t chunk = std::min(static_cast<size_t>(64 * 1024),
                                    compressed.size() - inPos);

            stream.next_in = &compressed[inPos];
            stream.avail_in = static_cast<uInt>(chunk);
            inPos += chunk;
        }

        stream.next_out = outbuf.data();
        stream.avail_out = static_cast<uInt>(outbuf.size());

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR)
        {
            inflateEnd(&stream);
            return {};
        }

        size_t have = outbuf.size() - stream.avail_out;
        if (have != 0U)
        {
            output.append(outbuf.begin(), outbuf.begin() + have);
        }

        if (ret == Z_STREAM_END)
        {
            break;
        }

        if (ret == Z_BUF_ERROR && stream.avail_in == 0)
        {
            // No progress possible and no more input
            inflateEnd(&stream);
            return {};
        }
    }

    inflateEnd(&stream);
    return output;
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
