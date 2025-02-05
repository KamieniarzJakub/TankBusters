resource "google_compute_firewall" "external-firewall" {
  name    = "${local.res_prefix}external-firewall"
  network = google_compute_network.vpc.name

  allow {
    protocol = "icmp"
  }

  allow {
    protocol = "tcp"
    ports    = ["22", "1234"]
  }

  source_ranges = ["0.0.0.0/0"]
}
