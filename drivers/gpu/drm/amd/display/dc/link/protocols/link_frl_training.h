/*
 * HDMI 2.1 Fixed Rate Link (FRL) training
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#ifndef __LINK_HDMI_FRL_TRAINING_H__
#define __LINK_HDMI_FRL_TRAINING_H__

#include "link_service.h"

struct dc_link;
struct link_resource;

/*
 * Perform HDMI 2.1 FRL link training.
 *
 * @link:
 *   DC link representing the HDMI connection.
 *
 * @link_res:
 *   Link resources containing the HPO HDMI FRL link encoder.
 *
 * @frl_rate:
 *   FRL rate index as defined by HDMI 2.1 (1–6).
 *
 * @lane_count:
 *   Number of FRL lanes (3 or 4).
 *
 * Returns:
 *   true  - training completed successfully, link is active
 *   false - training failed, link is disabled
 */
bool dc_link_perform_frl_training(struct dc_link *link,
				  const struct link_resource *link_res);

#endif /* __LINK_HDMI_FRL_TRAINING_H__ */
