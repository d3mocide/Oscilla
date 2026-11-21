/* Relative stationary-test diagnostics, without coordinates or field payloads. */
#include "app/deck_app.h"
#include <cstdio>
namespace app {
void DeckApp::gnssDiagnostic(const std::string &arg,uint32_t now)
{
    if(arg=="start")gnss_noise_.start();
    else if(arg=="stop")gnss_noise_.stop();
    else if(arg!="status"){log("gnss_test: use start, status or stop");return;}
    char out[384];
    snprintf(out,sizeof out,"gnss_test active=%u samples=%lu x_m=%.2f y_m=%.2f rms_m=%.2f raw_path_m=%.2f max_step_m=%.2f hdop=%.2f fix_age_ms=%lu chkfail=%lu overlong=%lu state=%s",
             gnss_noise_.active(),(unsigned long)gnss_noise_.samples(),gnss_noise_.x(),gnss_noise_.y(),gnss_noise_.rms(),gnss_noise_.path(),gnss_noise_.maxStep(),
             double(gnss_.fix().hdop),(unsigned long)gnss_.fixAgeMs(now),(unsigned long)gnss_parser_.checksumFailures(),(unsigned long)gnss_parser_.overlongLines(),gnssStateName(gnss_.state(now)));
    log(out);
}
}
