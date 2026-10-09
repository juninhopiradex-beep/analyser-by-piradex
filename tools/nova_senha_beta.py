#!/usr/bin/env python3
"""
Gera uma nova senha BETA para o ANALYSER by Piradex.

Uso:  python3 tools/nova_senha_beta.py 2027-03-31  [id]
  -> imprime a senha (para dar aos testers) e a linha a colar em kKeys
     no ficheiro Source/License.cpp. Depois é só fazer push (o GitHub compila).
"""
import hashlib, secrets, sys, datetime

ITER = 60000                     # tem de ser igual a License::kIterations
ALFA = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

if len(sys.argv) < 2:
    sys.exit(__doc__)
data = datetime.date.fromisoformat(sys.argv[1])
ident = sys.argv[2] if len(sys.argv) > 2 else "beta-" + secrets.token_hex(2)
senha = "BETA-" + "-".join("".join(secrets.choice(ALFA) for _ in range(4)) for _ in range(3))
salt = secrets.token_bytes(16)
h = hashlib.pbkdf2_hmac("sha256", senha.encode(), salt, ITER)

print(f"\nSenha beta (válida até {data:%d/%m/%Y}):  {senha}\n")
print("Linha para Source/License.cpp (dentro de kKeys):\n")
print(f'    {{ "{ident}", Kind::Trial,\n      "{salt.hex()}",\n      "{h.hex()}", {data.year}, {data.month}, {data.day} }},\n')
