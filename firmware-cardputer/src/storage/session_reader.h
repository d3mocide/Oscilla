/* Cooperative, read-only SD session review under the shared bus lock. */
#pragma once
#include "model/session_review.h"
namespace storage {
void sessionCatalog(model::SessionReview &review);
void sessionOpen(model::SessionReview &review);
void sessionPage(model::SessionReview &review, size_t page);
void sessionCancel(model::SessionReview &review);
/* One directory entry or <=512 bytes per call; parsing happens after unlock. */
void sessionTick(model::SessionReview &review, uint32_t now);
} // namespace storage
