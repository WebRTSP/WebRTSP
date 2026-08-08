#include "Request.h"


namespace rtsp {

void SetBearerAuthorization(Request* request, const std::string& token)
{
#if __cplusplus > 202603L
    request->headerFields.insert_or_assign(AuthorizationFieldName, "Bearer " + token);
#else
    request->headerFields[std::string(AuthorizationFieldName)] = "Bearer " + token;
#endif
}

}
