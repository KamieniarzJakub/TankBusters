resource "google_compute_address" "addr" {
  name         = "addr"
  address_type = "EXTERNAL"
  network_tier = "STANDARD"

  depends_on = [google_project_service.api]
}
