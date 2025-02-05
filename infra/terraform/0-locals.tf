locals {
  project_id = "terraform-tutorial-448821"
  region     = "us-east1" # https://cloud.google.com/free/docs/free-cloud-features#compute
  zone       = "us-east1-a"
  res_prefix = "tankbusters-"
  apis = [
    "compute.googleapis.com"
  ]
}
