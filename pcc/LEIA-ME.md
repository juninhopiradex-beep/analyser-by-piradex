# PCC — ligar o ANALYSER, o MIXSPACE e o LEVEL-X (uma vez)

Os plugins (ANALYSER 2.1.0, MIXSPACE 2.1.0, LEVEL-X 1.4.0) já aceitam chaves `ANL-`, `MXS-` e `LVX-`.
Falta ativá-las no servidor PCC (projeto Supabase **pcc**):

1. **Supabase → SQL Editor:** correr `supabase/migrations/20261124_analyser_mixspace_levelx_license.sql`
   (cria `pcc_new_analyser_license`, `pcc_new_mixspace_license`, `pcc_new_levelx_license`).
2. **Edge Function `validate`:** publicar `supabase/functions/validate/index.ts`
   (`supabase functions deploy validate --no-verify-jwt`). É igual à versão 5 que está no ar,
   só com `ANALYSER`, `MIXSPACE` e `LEVELX` acrescentados à lista `PRODUCTS`.
3. Criar chaves: `select * from pcc_new_levelx_license('Nome','email','País','venda', null);`
