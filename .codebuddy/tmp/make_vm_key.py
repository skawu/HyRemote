"""Generate the keypair for the Linux VM, outside the repository, and print only the public line.

ssh-keygen repeatedly mis-parsed the empty passphrase argument in this shell, so the key is generated with paramiko
instead. The private key is written under the user profile's .ssh directory - never inside the repository - and is never
printed. Only the OpenSSH public line is emitted, for a single paste into the VM's authorized_keys.
"""
import os
from pathlib import Path

import paramiko

priv = Path(os.environ["USERPROFILE"]) / ".ssh" / "hyremote_vm_rsa"
pub = priv.with_suffix(".rsa.pub")

if priv.exists():
    print(f"private key already present at {priv}")
    key = paramiko.RSAKey.from_private_key_file(str(priv))
else:
    key = paramiko.RSAKey.generate(4096)
    key.write_private_key_file(str(priv))
    print(f"private key written to {priv} (outside the repository)")
    try:
        os.chmod(priv, 0o600)
    except OSError:
        pass

public_line = f"{key.get_name()} {key.get_base64()} hyremote-vm-agent"
pub.write_text(public_line + "\n", encoding="utf-8")
print("=== PUBLIC KEY - paste this single line into the VM ===")
print(public_line)
print("=== fingerprint ===")
print(key.get_fingerprint().hex() if isinstance(key.get_fingerprint(), bytes) else key.get_fingerprint())
