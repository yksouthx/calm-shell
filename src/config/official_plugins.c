#include "config/official_plugins.h"

#include <string.h>

static const char *git_manifest(void) {
    return
        "name = \"git\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Git status-aware aliases and shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `git` -- no other commands, no network access beyond what git itself performs\"]\n"
        "\n"
        "[aliases]\n"
        "gs = \"git status -sb\"\n"
        "ga = \"git add\"\n"
        "gaa = \"git add --all\"\n"
        "gc = \"git commit\"\n"
        "gcm = \"git commit -m\"\n"
        "gco = \"git checkout\"\n"
        "gb = \"git branch\"\n"
        "gp = \"git push\"\n"
        "gl = \"git pull\"\n"
        "glog = \"git log --oneline --graph --decorate -20\"\n"
        "gd = \"git diff\"\n"
        "gds = \"git diff --staged\"\n";
}

static const char *docker_manifest(void) {
    return
        "name = \"docker\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Docker and docker-compose shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `docker`/`docker compose` -- can start, stop, and remove containers and images\"]\n"
        "\n"
        "[aliases]\n"
        "d = \"docker\"\n"
        "dc = \"docker compose\"\n"
        "dps = \"docker ps\"\n"
        "dpsa = \"docker ps -a\"\n"
        "dimg = \"docker images\"\n"
        "dclean = \"docker system prune -f\"\n"
        "dlogs = \"docker logs -f\"\n"
        "dexec = \"docker exec -it\"\n"
        "\n"
        "[functions]\n"
        "dsh = \"\"\"\n"
        "docker exec -it \"$1\" sh\n"
        "\"\"\"\n";
}

static const char *node_manifest(void) {
    return
        "name = \"node\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Node.js / npm shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `npm`/`npx` -- can install packages and execute arbitrary JS\"]\n"
        "\n"
        "[aliases]\n"
        "ni = \"npm install\"\n"
        "nid = \"npm install --save-dev\"\n"
        "nr = \"npm run\"\n"
        "ns = \"npm start\"\n"
        "nt = \"npm test\"\n"
        "nb = \"npm run build\"\n"
        "nx = \"npx\"\n";
}

static const char *python_manifest(void) {
    return
        "name = \"python\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Python / venv / pip shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `python3`/`pip3` -- can install packages and execute arbitrary Python\"]\n"
        "\n"
        "[aliases]\n"
        "py = \"python3\"\n"
        "pip = \"pip3\"\n"
        "venv = \"python3 -m venv .venv\"\n"
        "activate = \"source .venv/bin/activate\"\n"
        "\n"
        "[functions]\n"
        "pyserve = \"\"\"\n"
        "python3 -m http.server \"${1:-8000}\"\n"
        "\"\"\"\n";
}

static const char *rust_manifest(void) {
    return
        "name = \"rust\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Cargo shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `cargo` -- can build, execute, and download crates for the current project\"]\n"
        "\n"
        "[aliases]\n"
        "cb = \"cargo build\"\n"
        "cr = \"cargo run\"\n"
        "ct = \"cargo test\"\n"
        "cch = \"cargo check\"\n"
        "cbr = \"cargo build --release\"\n"
        "cfmt = \"cargo fmt\"\n"
        "ccl = \"cargo clippy\"\n";
}

static const char *kubernetes_manifest(void) {
    return
        "name = \"kubernetes\"\n"
        "version = \"1.0.0\"\n"
        "description = \"kubectl shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `kubectl` against whatever cluster your current context points at -- can read and "
        "modify cluster state\"]\n"
        "\n"
        "[aliases]\n"
        "k = \"kubectl\"\n"
        "kgp = \"kubectl get pods\"\n"
        "kgs = \"kubectl get svc\"\n"
        "kgd = \"kubectl get deployments\"\n"
        "kaf = \"kubectl apply -f\"\n"
        "kdel = \"kubectl delete -f\"\n"
        "klog = \"kubectl logs -f\"\n"
        "kctx = \"kubectl config current-context\"\n"
        "kns = \"kubectl config set-context --current --namespace\"\n";
}

static const char *ssh_manifest(void) {
    return
        "name = \"ssh\"\n"
        "version = \"1.0.0\"\n"
        "description = \"SSH key and config shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `ssh`/`ssh-keygen` -- can open outbound network connections to hosts you specify\"]\n"
        "\n"
        "[aliases]\n"
        "sshconf = \"cat ~/.ssh/config\"\n"
        "sshls = \"ls -la ~/.ssh\"\n"
        "\n"
        "[functions]\n"
        "sshk = \"\"\"\n"
        "ssh-keygen -t ed25519 -C \"$1\"\n"
        "\"\"\"\n";
}

static const char *sysmon_manifest(void) {
    return
        "name = \"sysmon\"\n"
        "version = \"1.0.0\"\n"
        "description = \"System monitoring shortcuts (process, memory, disk usage)\"\n"
        "dependencies = []\n"
        "permissions = [\"runs read-only system inspection commands (ps, df, free) -- no destructive actions\"]\n"
        "\n"
        "[aliases]\n"
        "dfh = \"df -h\"\n"
        "duh = \"du -h -d 1\"\n"
        "meminfo = \"free -h\"\n"
        "psmem = \"ps aux --sort=-%mem\"\n"
        "pscpu = \"ps aux --sort=-%cpu\"\n";
}

static const char *nettools_manifest(void) {
    return
        "name = \"nettools\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Network inspection shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `curl`/`ss`/`ping` -- `myip` makes an outbound HTTPS request to determine your "
        "public IP\"]\n"
        "\n"
        "[aliases]\n"
        "myip = \"curl -s https://ifconfig.me\"\n"
        "ports = \"ss -tulpn\"\n"
        "\n"
        "[functions]\n"
        "portcheck = \"\"\"\n"
        "nc -zv \"$1\" \"$2\"\n"
        "\"\"\"\n";
}

static const char *archive_manifest(void) {
    return
        "name = \"archive\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Archive/compression shortcuts\"\n"
        "dependencies = []\n"
        "permissions = [\"runs `tar`/`zip`/`gzip` on files you specify -- no network access\"]\n"
        "\n"
        "[aliases]\n"
        "untar = \"tar -xvf\"\n"
        "mktar = \"tar -cvf\"\n"
        "ungz = \"gunzip\"\n"
        "unzipd = \"unzip -d\"\n"
        "\n"
        "[functions]\n"
        "extract = \"\"\"\n"
        "case \"$1\" in\n"
        "  *.tar.gz|*.tgz) tar -xzvf \"$1\" ;;\n"
        "  *.tar) tar -xvf \"$1\" ;;\n"
        "  *.zip) unzip \"$1\" ;;\n"
        "  *.gz) gunzip \"$1\" ;;\n"
        "  *.bz2) bunzip2 \"$1\" ;;\n"
        "  *) echo \"extract: unknown archive type for $1\" ;;\n"
        "esac\n"
        "\"\"\"\n";
}

static const OfficialPlugin OFFICIAL_PLUGINS[10] = {
    {"git", "Git status-aware aliases and shortcuts", git_manifest},
    {"docker", "Docker and docker-compose shortcuts", docker_manifest},
    {"node", "Node.js / npm shortcuts", node_manifest},
    {"python", "Python / venv / pip shortcuts", python_manifest},
    {"rust", "Cargo shortcuts", rust_manifest},
    {"kubernetes", "kubectl shortcuts", kubernetes_manifest},
    {"ssh", "SSH key and config shortcuts", ssh_manifest},
    {"sysmon", "System monitoring shortcuts (process, memory, disk usage)", sysmon_manifest},
    {"nettools", "Network inspection shortcuts", nettools_manifest},
    {"archive", "Archive/compression shortcuts", archive_manifest},
};

const OfficialPlugin *official_plugins(size_t *out_count) {
    if (out_count) {
        *out_count = sizeof(OFFICIAL_PLUGINS) / sizeof(OFFICIAL_PLUGINS[0]);
    }
    return OFFICIAL_PLUGINS;
}

const OfficialPlugin *official_plugin_find(const char *name) {
    size_t count = 0;
    const OfficialPlugin *all = official_plugins(&count);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(all[i].name, name) == 0) {
            return &all[i];
        }
    }
    return NULL;
}
