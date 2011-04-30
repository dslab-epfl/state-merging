====================
Running in a Cluster
====================

Cloud9 supports running on private clusters of commodity hardware, as well as on public cloud infrastructures.  We provide in the `infra/` directory a handy set of Python scripts that coordinate Cloud9 execution across a set of machines accessible via SSH through their IP addresses.  Therefore, these scripts can be used both on private clusters, and on public cloud instances (the nature of the machines is orthogonal to the scripts, as long as they are available through their IP addresses).

In this tutorial, we show how the infrastructure scripts can be used to schedule and run Cloud9 across the entire range of Coreutils.


Setting Up Worker Nodes and Load Balancer
=========================================

The infrastructure scripts run the Cloud9 workers on the cluster machines, while the load balancer is run locally from where the scripts are being invoked.  Therefore, you need to follow the Cloud9 installation procedure on all the machines that Cloud9 will run on.  You may also want to consider setting up Cloud9 in a single place, and make the installation avaiable to the other machines via NFS.
