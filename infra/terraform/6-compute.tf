variable "machine_type" {
  default = "e2-micro"
  type    = string
}

variable "machine_image" {
  default = "debian-cloud/deiban-12"
  type    = string
}

resource "google_compute_instance" "default" {
  name         = "${local.res_prefix}vm"
  machine_type = var.machine_type
  zone         = local.zone

  boot_disk {
    initialize_params {
      image = var.machine_image
    }
  }

  network_interface {
    network = google_compute_network.vpc.id
    access_config {
      nat_ip = google_compute_address.addr.address
    }
  }
}

output "instance_public_ip" {
  description = "Public IP of compute instance"
  value       = google_compute_address.addr.address
}
