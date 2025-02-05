resource "google_compute_network" "vpc" {
  name                            = "${local.res_prefix}vpc"
  routing_mode                    = "REGIONAL"
  auto_create_subnetworks         = false
  delete_default_routes_on_create = true
  depends_on                      = [google_project_service.api]
}

resource "google_compute_route" "default_route" {
  name             = "${local.res_prefix}default-route"
  dest_range       = "0.0.0.0/0"
  network          = google_compute_network.vpc.name
  next_hop_gateway = "default-internet-gateway"
}


