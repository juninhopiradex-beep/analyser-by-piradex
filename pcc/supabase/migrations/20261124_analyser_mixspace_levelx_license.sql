-- PCC: licencas para ANALYSER (ANL-), MIXSPACE (MXS-) e LEVEL-X (LVX-), iguais a pcc_new_delex_license.
-- Correr uma vez no Supabase -> SQL Editor (projeto "pcc").
create or replace function public.pcc_new_analyser_license(p_cliente text, p_email text, p_pais text, p_tipo text, p_validade date)
returns table(chave text, validade date) language plpgsql security definer set search_path to 'public','extensions' as $function$
declare k text;
begin
  k := 'ANL' || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4)) || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4)) || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4));
  insert into public.licencas(cliente,email,pais,plugin,chave,tipo,validade,estado)
  values (p_cliente,p_email,p_pais,'analyser',k,coalesce(p_tipo,'venda'),p_validade,'ativo');
  return query select k, p_validade;
end $function$;

create or replace function public.pcc_new_mixspace_license(p_cliente text, p_email text, p_pais text, p_tipo text, p_validade date)
returns table(chave text, validade date) language plpgsql security definer set search_path to 'public','extensions' as $function$
declare k text;
begin
  k := 'MXS' || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4)) || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4)) || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4));
  insert into public.licencas(cliente,email,pais,plugin,chave,tipo,validade,estado)
  values (p_cliente,p_email,p_pais,'mixspace',k,coalesce(p_tipo,'venda'),p_validade,'ativo');
  return query select k, p_validade;
end $function$;

create or replace function public.pcc_new_levelx_license(p_cliente text, p_email text, p_pais text, p_tipo text, p_validade date)
returns table(chave text, validade date) language plpgsql security definer set search_path to 'public','extensions' as $function$
declare k text;
begin
  k := 'LVX' || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4)) || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4)) || '-' || upper(substr(encode(gen_random_bytes(2),'hex'),1,4));
  insert into public.licencas(cliente,email,pais,plugin,chave,tipo,validade,estado)
  values (p_cliente,p_email,p_pais,'levelx',k,coalesce(p_tipo,'venda'),p_validade,'ativo');
  return query select k, p_validade;
end $function$;

revoke all on function public.pcc_new_analyser_license(text,text,text,text,date) from public, anon;
revoke all on function public.pcc_new_mixspace_license(text,text,text,text,date) from public, anon;
revoke all on function public.pcc_new_levelx_license(text,text,text,text,date) from public, anon;
grant execute on function public.pcc_new_analyser_license(text,text,text,text,date) to authenticated, service_role;
grant execute on function public.pcc_new_mixspace_license(text,text,text,text,date) to authenticated, service_role;
grant execute on function public.pcc_new_levelx_license(text,text,text,text,date) to authenticated, service_role;
