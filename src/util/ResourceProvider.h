#ifndef LRPARSER_RESOURCE_PROVIDER_H
#define LRPARSER_RESOURCE_PROVIDER_H

namespace util {

// Any object holding a reference to the provider can ask it to allocate a
// resource and keep it available through the lifetime of the provider. The
// provider is the only owner of the resource.
template <class ResourceT> struct ResourceProvider {
    virtual ResourceT *requestResource() = 0;
};

} // namespace util

#endif